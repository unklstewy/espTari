import fs from "node:fs";
import path from "node:path";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const workspaceRoot = path.resolve(process.env.ESPTARI_MCP_ROOT || path.join(process.cwd()));
const allowedRoots = [
  path.resolve(workspaceRoot, "TRACKING"),
  path.resolve(workspaceRoot, "docs"),
];

function isAllowed(candidatePath) {
  const resolved = path.resolve(candidatePath);
  return allowedRoots.some((root) => resolved === root || resolved.startsWith(`${root}${path.sep}`));
}

function toRelative(absolutePath) {
  return path.relative(workspaceRoot, absolutePath).split(path.sep).join("/");
}

function resolveUserPath(userPath = "") {
  const candidate = userPath
    ? (path.isAbsolute(userPath) ? userPath : path.join(workspaceRoot, userPath))
    : workspaceRoot;
  const resolved = path.resolve(candidate);
  if (!isAllowed(resolved)) {
    throw new Error("Path is outside allowed roots (TRACKING, docs)");
  }
  return resolved;
}

const server = new McpServer({
  name: "esptari-tracking-docs",
  version: "1.0.0",
});

server.registerTool(
  "ping",
  {
    description: "Health check for MCP connectivity",
    inputSchema: {},
  },
  async () => ({
    content: [
      {
        type: "text",
        text: JSON.stringify({
          ok: true,
          server: "esptari-tracking-docs",
          workspace_root: workspaceRoot,
          allowed_roots: allowedRoots.map((p) => toRelative(p)),
        }),
      },
    ],
  }),
);

server.registerTool(
  "list_allowed_roots",
  {
    description: "List workspace-relative roots this server permits",
    inputSchema: {},
  },
  async () => ({
    content: [{ type: "text", text: JSON.stringify(allowedRoots.map((p) => toRelative(p))) }],
  }),
);

server.registerTool(
  "list_files",
  {
    description: "List files/folders under TRACKING or docs",
    inputSchema: {
      path: z.string().optional(),
    },
  },
  async ({ path: userPath }) => {
    const base = userPath ? resolveUserPath(userPath) : workspaceRoot;
    const stat = fs.existsSync(base) ? fs.statSync(base) : null;
    if (!stat) {
      throw new Error("Path does not exist");
    }

    if (stat.isFile()) {
      return { content: [{ type: "text", text: toRelative(base) }] };
    }

    const entries = fs
      .readdirSync(base)
      .map((name) => path.join(base, name))
      .filter((p) => isAllowed(p))
      .sort((a, b) => a.localeCompare(b))
      .map((p) => `${toRelative(p)}${fs.statSync(p).isDirectory() ? "/" : ""}`);

    return { content: [{ type: "text", text: entries.join("\n") }] };
  },
);

server.registerTool(
  "read_text_file",
  {
    description: "Read text file content by line range (1-based inclusive)",
    inputSchema: {
      path: z.string(),
      start_line: z.number().int().min(1).optional(),
      end_line: z.number().int().min(1).optional(),
    },
  },
  async ({ path: userPath, start_line = 1, end_line = 400 }) => {
    const filePath = resolveUserPath(userPath);
    if (!fs.existsSync(filePath)) {
      throw new Error("File does not exist");
    }
    if (!fs.statSync(filePath).isFile()) {
      throw new Error("Path is not a file");
    }

    const lines = fs.readFileSync(filePath, "utf8").split(/\r?\n/);
    const start = Math.max(1, start_line);
    const end = Math.max(start, end_line);
    const slice = lines.slice(start - 1, end);
    const numbered = slice.map((line, index) => `${start + index}: ${line}`);
    return { content: [{ type: "text", text: numbered.join("\n") }] };
  },
);

server.registerTool(
  "search_text",
  {
    description: "Case-insensitive text search within TRACKING/docs",
    inputSchema: {
      query: z.string().min(1),
      path: z.string().optional(),
    },
  },
  async ({ query, path: userPath }) => {
    const root = userPath ? resolveUserPath(userPath) : workspaceRoot;
    const needle = query.toLowerCase();
    const matches = [];

    function walk(currentPath) {
      if (!isAllowed(currentPath)) {
        return;
      }
      const stat = fs.statSync(currentPath);
      if (stat.isFile()) {
        try {
          const lines = fs.readFileSync(currentPath, "utf8").split(/\r?\n/);
          for (let i = 0; i < lines.length; i += 1) {
            if (lines[i].toLowerCase().includes(needle)) {
              matches.push(`${toRelative(currentPath)}:${i + 1}:${lines[i]}`);
              if (matches.length >= 500) {
                return;
              }
            }
          }
        } catch {
          return;
        }
        return;
      }

      for (const child of fs.readdirSync(currentPath)) {
        if (matches.length >= 500) {
          break;
        }
        walk(path.join(currentPath, child));
      }
    }

    walk(root);
    return { content: [{ type: "text", text: matches.join("\n") }] };
  },
);

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((error) => {
  console.error("MCP server error:", error);
  process.exit(1);
});
