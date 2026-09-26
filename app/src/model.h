/*
 * model.h
 *
 * Simple model management for RC remote controller application.
 * A "model" is a named set of settings (opaque byte buffer).
 * Supports up to MODEL_MAX_COUNT models.
 */
#ifndef RC_REMOTE_CONTROLLER_MODEL_H
#define RC_REMOTE_CONTROLLER_MODEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#ifndef MODEL_MAX_COUNT
#define MODEL_MAX_COUNT 6
#endif

#ifndef MODEL_NAME_MAX
#define MODEL_NAME_MAX 32
#endif

#ifndef MODEL_SELECTION_COUNT
#define MODEL_SELECTION_COUNT 4
#endif

/* Channel mapping function type and structure
 * These match the application's channel mapping concept and allow
 * models to store per-channel mapping settings directly.
 */
typedef uint8_t (*map_t)(
    const uint16_t min,
    const uint16_t max,
    const uint16_t center,
          uint16_t data,
    const bool is_reversed);

struct channel_map {
    uint16_t min;
    uint16_t max;
    uint16_t center;
    uint16_t input;
    bool is_reversed;
    map_t map;
};

/* Model stores a named `struct channel_map` as its payload. */
typedef struct {
    bool used;                          /* slot in use */
    char name[MODEL_NAME_MAX];          /* NUL-terminated name */
    struct channel_map ch[MAX_CHANNELS];              /* channel mapping data */
    uint8_t channel_selection[MODEL_SELECTION_COUNT]; /* ch1..ch4 selected source channel */
} model_t;

/* The persistent model list is placed in a dedicated linker section.
 * Using `.rodata.model_data` keeps the array in read-only flash storage,
 * and __attribute__((used)) prevents the linker from discarding it.
 */
extern model_t active_model;

extern uint8_t def_map(
	const uint16_t min,
	const uint16_t max,
	const uint16_t center,
	      uint16_t data,
	const bool is_reversed);

/* Create a new model with the given name .
 * Returns 0 or error code.
 */
int model_create(const char *name);

/* Store active model in persistent Storage. 
 * Returns 0 on success, error code on error
 */
int model_save(const struct channel_map *map, const uint8_t *sel);

/* Remove a model at index. Returns 0 on success, error code on error. */
int model_remove(int index);

/**
 * @brief Load the active model from persistent storage.
 *
 * Loads the model identified by the given name into the active model.
 * Sets UI parameters from active_model.
 *
 * @param name Pointer to model name.
 * @return 0 or error code.
 */
 * @return 0 or error code.
 */
int load_model(const char *model_name);

#ifdef __cplusplus
}
#endif

#endif /* RC_REMOTE_CONTROLLER_MODEL_H */
