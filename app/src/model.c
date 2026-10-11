/*
 * model.c
 *
 * Persistent model storage section definition.
 * The model array is placed in a dedicated linker section so it can be
 * stored in flash / read-only storage.
 */

#include "model.h"
#include "model_fs.h"
#include "vars.h"
#include <zephyr/logging/log.h>

#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(model, CONFIG_LOG_DEFAULT_LEVEL);

/* This variable holds the currently active model data used by the UI.
 * It also holds rf data from the remote controller.
 */

model_t active_model;

/* this is a temporary model used for creating or adding new models
 * into the persistent storage */

static model_t new_model;

/**
 * @brief Create a new model with default settings.
 *
 * use local static temporary model.
 *
 * @param name Null-terminated model name.
 * @return 0 or error code.
 */
static int model_create(const char *name)
{
    memset(new_model.name, '\0', MODEL_NAME_MAX);

    for (uint8_t j = 0; j < MAX_CHANNELS; j++) {
        new_model.ch[j].min = 0+CALIBRATION_OFFSET;
        new_model.ch[j].max = ADC_MAX_VALUE-CALIBRATION_OFFSET;
        new_model.ch[j].center = ADC_MAX_VALUE / 2;
        new_model.ch[j].input = ADC_MAX_VALUE / 2;
        new_model.ch[j].is_reversed = false;
        new_model.ch[j].map = def_map;
    }
            
    /* Default CH1..CH4 selections: ROULIS, TANGAGE, GAZ, LACET. */
    new_model.channel_selection[0] = ROULIS;
    new_model.channel_selection[1] = TANGAGE;
    new_model.channel_selection[2] = GAZ;
    new_model.channel_selection[3] = LACET;
    new_model.used = true;
    strncpy(new_model.name, name, MODEL_NAME_MAX - 1);
    new_model.name[MODEL_NAME_MAX - 1] = '\0';

    return 0;
}
/**
 * @brief Initialize the model system and load the active model.
 * if no model is found, create a default model.
 * 
 * @param none
 * 
 * @return 0 on success, error code on failure.
 */ 
int model_init(void)
{
    char buf[MODEL_MAX_COUNT][MODEL_NAME_MAX] = {0};
	size_t models_num = 0;
    int rc = model_fs_init();
    
	if (rc != 0) {
		LOG_ERR("model_fs_init failed (%d)", rc);
		return rc;
	}

	rc = model_fs_list_models(buf, &models_num);
	if (rc < 0) {
		LOG_ERR("model_fs_list_models failed (%d)", rc);
		return rc;
	}

	if (models_num == 0) {
		LOG_INF("No models found, creating default model");
		rc = model_add("Default Model\0");
		if (rc != 0) {
			LOG_ERR("Failed to create default model");
			return rc;
		}
		LOG_INF("Default model created successfully");

		rc = model_fs_list_models(buf, &models_num);
		if (rc < 0 || models_num == 0U) {
			LOG_ERR("Failed to list newly created default model: %d", rc);
			return rc < 0 ? rc : -EIO;
		}
	} else {
		LOG_INF("%zu models found", models_num);
	}
	/* Set the first model as the active device */
	rc = model_load(buf[0]);
	if (rc != 0) {
		LOG_ERR("Failed to load initial model");
		return rc;
	}

	/* Populate device list from file system */
	for (size_t i = 0; i < models_num; i++) {
    	set_var_device_list(buf[i]);
	}
    return 0;
}
/**
 * @brief Add a new model with the given name.
 * 
 * @param name Null-terminated model name.
 * @return 0 on success, error code on failure.
 */
int model_add(const char *name)
{
    int rc = model_create(name);
    if (rc != 0) {
        LOG_ERR("Failed to create model '%s': %d", name, rc);
        return rc;
    }
    rc = model_fs_save_model(name, &new_model);
    if (rc != 0) {
        LOG_ERR("Failed to save model '%s' to storage: %d", name, rc);
        return rc;
    }
    /* Set the newly created model as the active device */
    model_load(name);
    /* Update device list in UI */
    set_var_device_list(name);

    return 0;
}


/**
 * @brief Load the active model channel map.
 *
 * Loads the model identified by the given name into the active model
 * Set UI parameters from active_model.
 *
 * @param name Pointer to model name.
 * @return 0 or error code.
 */
int model_load(const char *model_name)
{
    int rc = model_fs_load_model(model_name, &active_model);

    if (rc != 0) {
        LOG_ERR("Failed to load model '%s' from storage: %d", model_name, rc);
        return rc;
    }
    memcpy(active_model.name, model_name, MODEL_NAME_MAX);

    /* Update UI variables with the loaded model's channel selection */
    set_var_selection1(active_model.channel_selection[0]);
	set_var_selection2(active_model.channel_selection[1]);
	set_var_selection3(active_model.channel_selection[2]);
	set_var_selection4(active_model.channel_selection[3]);
    active_model.used = true; // Ensure the slot is marked as used
    set_var_device_name(active_model.name);
	
	return rc;
}

/**
 * @brief Save channel map data into an existing model slot.
 *
 * @param data Pointer to channel map data.
 * @return 0 on success, error code on failure.
 */
int model_store(const struct channel_map *map, const uint8_t *sel)
{
    active_model.used = true; // Ensure the slot is marked as used
    memcpy(active_model.name, get_var_device_name(), MODEL_NAME_MAX);
    memcpy(active_model.ch, map, sizeof(active_model.ch));
    if (sel != NULL) {
        memcpy(active_model.channel_selection,
               sel,
               MODEL_SELECTION_COUNT * sizeof(active_model.channel_selection[0]));
    }
    /* Save the model to persistent storage */
    return model_fs_save_model(active_model.name, &active_model);
}

/**
 * @brief Remove a model from persistent storage by name.
 * Warning: if model to remove is the active model
 * active_model will hold another model from persistent storage
 * or will hold a default model with default values if no other model
 * is found
 * 
 * @param model_name Pointer to the name of the model to remove.
 * @return 0 on success, error code on failure.
 */
int model_remove(char *model_name)
{
    int rc = 0;

    /* check if model name corresponds to active_model.name */
    if (strcmp(model_name, active_model.name) == 0) {
        /* check if another model exists by listing models */
        char model_list[MODEL_MAX_COUNT][MODEL_NAME_MAX] = {0};
        size_t model_count = 0;

        /* First of all, remove the model from persistent storage */
        rc = model_fs_delete_model(model_name);
        /* Refresh the device list */
        set_var_device_list("");

        if (rc != 0) {
            LOG_ERR("Failed to remove model '%s' from storage: %d", model_name, rc);
            return rc;
        }
        rc = model_fs_list_models(model_list, &model_count);
        if (rc != 0) {
            LOG_ERR("Failed to list models: %d", rc);
            return rc;
        }
        if (model_count > 0) {
            /* load the first model that is not the one being removed */
            for (size_t i = 0; i < model_count; i++) {
                if (strcmp(model_list[i], model_name) != 0) {
                    rc = model_load(model_list[i]);
                    if (rc != 0) {
                        LOG_ERR("Failed to load model '%s': %d", model_list[i], rc);
                        return rc;
                    }
                    break;
                }
            }
        } else {
            /* No other models exist, create a default model */
            LOG_INF("No other models found, creating default model");
            model_create("Default Model");
            rc = model_store(new_model.ch, new_model.channel_selection);
            set_var_device_list("");
            if (rc != 0) {
                LOG_ERR("Failed to save default model: %d", rc);
                return rc;
            }
        }
    } else {
        /* If the model being removed is not the active model, do nothing */
        LOG_INF("Removing model '%s' which is not the active model", model_name);
        rc = model_fs_delete_model(model_name);
        set_var_device_list("");
        if (rc != 0) {
            LOG_ERR("Failed to remove model '%s' from storage: %d", model_name, rc);
            return rc;
        }
    }
    return rc; // Success
}

