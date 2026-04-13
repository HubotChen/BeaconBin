all-sensors chip includes a crude simulation of a NAU7802 and a VL53L5CX.

In order to simplify the simulation, both devices supply a 32-bit value to the I2C bus. This is not reflective of the real chips, since full implementation of the real chips would be too prohibitive and computationally intensive