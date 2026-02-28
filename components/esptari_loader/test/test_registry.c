/**
 * @file test_registry.c
 * @brief Unit tests for component registry
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "unity.h"
#include "esp_err.h"
#include "esptari_loader.h"

/*===========================================================================*/
/* Test: Loader Initialization                                               */
/*===========================================================================*/

TEST_CASE("Loader initializes successfully", "[registry]")
{
    esp_err_t err = loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Shutdown cleanly */
    loader_shutdown();
}

TEST_CASE("Loader double init returns OK", "[registry]")
{
    esp_err_t err = loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    /* Second init should also succeed (idempotent) */
    err = loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    loader_shutdown();
}

/*===========================================================================*/
/* Test: Component Listing                                                   */
/*===========================================================================*/

TEST_CASE("Empty registry returns zero components", "[registry]")
{
    loader_init();
    
    component_info_t infos[8];
    size_t count = 99;  /* Initialize to non-zero */
    
    esp_err_t err = loader_list_components(infos, 8, &count);
    
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(0, count);
    
    loader_shutdown();
}

/*===========================================================================*/
/* Test: Invalid Arguments                                                   */
/*===========================================================================*/

TEST_CASE("loader_load_component rejects NULL path", "[registry]")
{
    loader_init();
    
    void *interface = NULL;
    esp_err_t err = loader_load_component(NULL, COMPONENT_TYPE_CPU, &interface);
    
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    TEST_ASSERT_NULL(interface);
    
    loader_shutdown();
}

TEST_CASE("loader_load_component rejects NULL interface_out", "[registry]")
{
    loader_init();
    
    esp_err_t err = loader_load_component("/sdcard/test.ebin", COMPONENT_TYPE_CPU, NULL);
    
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    loader_shutdown();
}

TEST_CASE("loader_get_info rejects NULL arguments", "[registry]")
{
    loader_init();
    
    component_info_t info;
    
    /* NULL interface */
    esp_err_t err = loader_get_info(NULL, &info);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    /* NULL info struct */
    int dummy_interface = 42;
    err = loader_get_info(&dummy_interface, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    loader_shutdown();
}

/*===========================================================================*/
/* Test: Unload Non-existent Component                                       */
/*===========================================================================*/

TEST_CASE("loader_unload_component with NULL returns error", "[registry]")
{
    loader_init();
    
    esp_err_t err = loader_unload_component(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    loader_shutdown();
}

TEST_CASE("loader_unload_component with unknown interface returns error", "[registry]")
{
    loader_init();
    
    int dummy = 123;
    esp_err_t err = loader_unload_component(&dummy);
    
    /* Should return NOT_FOUND since this interface isn't registered */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    
    loader_shutdown();
}

/*===========================================================================*/
/* Test: Component Type Values                                               */
/*===========================================================================*/

TEST_CASE("Component types have expected values", "[registry]")
{
    TEST_ASSERT_EQUAL(1, COMPONENT_TYPE_CPU);
    TEST_ASSERT_EQUAL(2, COMPONENT_TYPE_VIDEO);
    TEST_ASSERT_EQUAL(3, COMPONENT_TYPE_AUDIO);
    TEST_ASSERT_EQUAL(4, COMPONENT_TYPE_IO);
}

/*===========================================================================*/
/* Test: Load Non-existent File                                              */
/*===========================================================================*/

TEST_CASE("loader_load_component fails for non-existent file", "[registry]")
{
    loader_init();
    
    void *interface = NULL;
    esp_err_t err = loader_load_component("/sdcard/nonexistent.ebin", 
                                           COMPONENT_TYPE_CPU, 
                                           &interface);
    
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_NULL(interface);
    
    loader_shutdown();
}
