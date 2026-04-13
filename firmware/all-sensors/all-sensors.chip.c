#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  uint32_t distance;
  uint32_t weight;
  bool butt1;
  bool butt2;
} sim_frame_t;

const sim_frame_t sim_data[] = {
  // --- EVENT: SYSTEM START / EMPTY BIN ---
  {1000, 0, false, false}, {1001, 1, false, false}, {1000, 0, false, false},
  {999, 0, false, false},  {1000, 1, false, false}, {1001, 0, false, false},
  {1000, 0, false, false}, {1000, 0, false, false}, {999, 1, false, false},
  {1001, 0, false, false}, {1000, 0, false, false}, {1000, 0, false, false},

  // --- EVENT: ITEM 1 ADDED (Delta > 20) ---
  {850, 200, true, false},  // Action: Add + B1 Confirm
  {851, 199, false, false}, // B1 Released
  
  // --- IDLE TIME (10+ points) ---
  {850, 200, false, false}, {849, 201, false, false}, {851, 200, false, false},
  {850, 199, false, false}, {850, 200, false, false}, {850, 200, false, false},
  {849, 201, false, false}, {851, 200, false, false}, {850, 199, false, false},
  {850, 200, false, false}, {850, 200, false, false}, {851, 200, false, false},

  // --- EVENT: ITEM 2 ADDED (Delta > 20) ---
  {750, 450, false, true},  // Action: Add + B2 Confirm
  {751, 449, false, false}, // B2 Released

  // --- IDLE TIME (10+ points) ---
  {750, 450, false, false}, {751, 451, false, false}, {749, 449, false, false},
  {750, 450, false, false}, {750, 450, false, false}, {750, 450, false, false},
  {751, 451, false, false}, {749, 449, false, false}, {750, 450, false, false},
  {750, 450, false, false}, {750, 450, false, false}, {750, 450, false, false},

  // --- EVENT: ITEM 3 ADDED (Delta > 20) ---
  {600, 700, true, true},   // Action: Add + Both Confirm
  {601, 699, false, false}, // Released

  // --- IDLE TIME (10+ points) ---
  {600, 700, false, false}, {601, 701, false, false}, {599, 699, false, false},
  {600, 700, false, false}, {600, 700, false, false}, {600, 700, false, false},
  {601, 701, false, false}, {599, 699, false, false}, {600, 700, false, false},
  {600, 700, false, false}, {600, 700, false, false}, {600, 700, false, false},

  // --- EVENT: ACCIDENTAL BUTTON PRESS (No item change) ---
  {600, 700, true, false},  // B1 bumped
  {599, 701, false, false}, // Released
  
  // --- IDLE TIME (10+ points) ---
  {600, 700, false, false}, {601, 701, false, false}, {599, 699, false, false},
  {600, 700, false, false}, {600, 700, false, false}, {600, 700, false, false},
  {601, 701, false, false}, {599, 699, false, false}, {600, 700, false, false},
  {600, 700, false, false}, {600, 700, false, false}, {600, 700, false, false},

  // --- EVENT: ITEM REMOVED (Delta > 20) ---
  {750, 450, false, true},  // Action: Remove + B2 Confirm
  {749, 451, false, false}, // Released

  // --- IDLE TIME (10+ points) ---
  {750, 450, false, false}, {751, 451, false, false}, {749, 449, false, false},
  {750, 450, false, false}, {750, 450, false, false}, {750, 450, false, false},
  {751, 451, false, false}, {749, 449, false, false}, {750, 450, false, false},
  {750, 450, false, false}, {750, 450, false, false}, {750, 450, false, false},

  // --- EVENT: FINAL CLEAR (Return to Empty) ---
  {1000, 0, true, true},    // Action: Clear + Both Confirm
  {1001, 1, false, false},  // Released

  // --- FINAL IDLE TIME (10+ points) ---
  {1000, 0, false, false}, {999, 1, false, false}, {1000, 0, false, false},
  {1000, 0, false, false}, {1001, 0, false, false}, {1000, 1, false, false},
  {999, 0, false, false},  {1000, 0, false, false}, {1000, 0, false, false},
  {1001, 1, false, false}, {1000, 0, false, false}, {1000, 0, false, false}
};

#define SIM_DATA_LEN (sizeof(sim_data) / sizeof(sim_frame_t))

#define SIM_DATA_LEN (sizeof(sim_data) / sizeof(sim_frame_t))

typedef struct {
  pin_t pin_int;
  pin_t pin_butt1;
  pin_t pin_butt2;
  uint8_t addr_tof;
  uint8_t addr_load;
  uint32_t current_address;
  uint32_t sim_index;
  uint8_t read_index;
  
  // Internal storage for the "current" simulated values
  uint32_t live_distance;
  uint32_t live_weight;
} chip_state_t;

static bool on_i2c_connect(void *user_data, uint32_t address, bool is_read);
static uint8_t on_i2c_read(void *user_data);
static void on_timer_tick(void *user_data);

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->addr_tof = 0x29;
  chip->addr_load = 0x42; 
  chip->sim_index = 0;

  chip->pin_butt1 = pin_init("BUTT1", OUTPUT_LOW);
  chip->pin_butt2 = pin_init("BUTT2", OUTPUT_LOW);
  chip->pin_int = pin_init("INT", OUTPUT_HIGH);

  pin_t scl = pin_init("SCL", INPUT);
  pin_t sda = pin_init("SDA", INPUT);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0, 
    .scl = scl,
    .sda = sda,
    .connect = on_i2c_connect,
    .read = on_i2c_read,
  };
  i2c_init(&i2c_config);

  const timer_config_t timer_config = {
    .callback = on_timer_tick,
    .user_data = chip,
  };
  timer_t timer_id = timer_init(&timer_config);
  timer_start(timer_id, 1100000, true); // 1 second per step

  printf("Simulator sequence started...\n");
}

static void on_timer_tick(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  sim_frame_t current = sim_data[chip->sim_index];

  // Update internal state
  chip->live_distance = current.distance;
  chip->live_weight = current.weight;

  // Update hardware pins
  pin_write(chip->pin_butt1, current.butt1 ? HIGH : LOW);
  pin_write(chip->pin_butt2, current.butt2 ? HIGH : LOW);

  // Trigger Interrupt
  pin_write(chip->pin_int, LOW);

  printf("Frame %d: Dist %u, Weight %u\n", chip->sim_index, chip->live_distance, chip->live_weight);

  chip->sim_index = (chip->sim_index + 1) % SIM_DATA_LEN;
}

static bool on_i2c_connect(void *user_data, uint32_t address, bool is_read) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (address == chip->addr_tof || address == chip->addr_load) {
    chip->current_address = address;
    chip->read_index = 0; 
    return true; 
  }
  return false; 
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  
  // Read from our internal "live" variables instead of attributes
  uint32_t val = (chip->current_address == chip->addr_tof) ? 
                  chip->live_distance : chip->live_weight;
  
  uint8_t result;
  if (chip->read_index == 0)      result = (uint8_t)(val >> 24);
  else if (chip->read_index == 1) result = (uint8_t)(val >> 16);
  else if (chip->read_index == 2) result = (uint8_t)(val >> 8);
  else {
    result = (uint8_t)(val & 0xFF);
    pin_write(chip->pin_int, HIGH);
  }

  chip->read_index = (chip->read_index + 1) % 4;
  return result;
}
