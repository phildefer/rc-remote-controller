/*
 * model.c
 *
 * Persistent model storage section definition.
 * The model array is placed in a dedicated linker section so it can be
 * stored in flash / read-only storage.
 */

#include "model.h"
#include "vars.h"

#include <string.h>

/* Persistent storage for the active model index. A value of 0xFF means
 * "not set" and should cause initialization to the first model (index 0).
 * Placed in the same read-only model data section so tools/linker can
 * locate it in persistent storage imagery.
 */
model_t active_model;

/**
 * @brief Create a new model slot.
 *
 * Allocates the first available model slot, initializes the channel map
 * data to safe defaults, and stores the model name.
 *
 * @param name Null-terminated model name.
 * @return 0 or error code.
 */
int model_create(const char *name)
{
    memset(active_model.name, 0, MODEL_NAME_MAX);

    for (uint8_t j = 0; j < MAX_CHANNELS; j++) {
        active_model.ch[j].min = 0+CALIBRATION_OFFSET;
        active_model.ch[j].max = ADC_MAX_VALUE-CALIBRATION_OFFSET;
        active_model.ch[j].center = ADC_MAX_VALUE / 2;
        active_model.ch[j].input = ADC_MAX_VALUE / 2;
        active_model.ch[j].is_reversed = false;
        active_model.ch[j].map = def_map;
    }
            
    /* Default CH1..CH4 selections: ROULIS, TANGAGE, GAZ, LACET. */
    active_model.channel_selection[0] = ROULIS;
    active_model.channel_selection[1] = TANGAGE;
    active_model.channel_selection[2] = GAZ;
    active_model.channel_selection[3] = LACET;
    active_model.used = true;
    strncpy(active_model.name, name, MODEL_NAME_MAX - 1);
    active_model.name[MODEL_NAME_MAX - 1] = '\0';

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
int load_model(const char *model_name)
{
    uint8_t channel_selection[MODEL_SELECTION_COUNT] = {ROULIS, TANGAGE, GAZ, LACET};
    int rc = model_fs_load_model(model_name, &active_model);

    if (rc != 0) {
        LOG_ERR("Failed to load model '%s' from storage: %d", model_name, rc);
        return rc;
    }
    memcpy(active_model.name, model_name, MODEL_NAME_MAX);

    memcpy(channel_selection,
            active_model.channel_selection,
            MODEL_SELECTION_COUNT * sizeof(active_model.channel_selection[0]));
    set_var_selection1(channel_selection[0]);
	set_var_selection2(channel_selection[1]);
	set_var_selection3(channel_selection[2]);
	set_var_selection4(channel_selection[3]);
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
int model_save(const struct channel_map *map, const uint8_t *sel)
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
 * @brief Remove a model slot 
 * Warning this function does not manage the active_model_index,
 * so if you remove the active model, you should set the
 * active_model_index to 0xFF or another valid index.
 *
 * Marks the model slot as unused and clears its data.
 * @param index Model index to remove.
 * @return 0 on success, error code on failure.
 */
uint8_t model_remove(int index)
{
    if (index >= MODEL_MAX_COUNT) {
        return 1; // Invalid index
    }

    model_storage[index].used = false;
    memset(model_storage[index].name, 0, MODEL_NAME_MAX);
    memset(model_storage[index].ch, 0, sizeof(model_storage[index].ch));

    return 0; // Success
}

