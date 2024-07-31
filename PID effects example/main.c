/*******************************************************
 HIDAPI - Multi-Platform library for
 communication with HID devices.

 Alan Ott
 Signal 11 Software

 libusb/hidapi Team

 Copyright 2022.

 This contents of this file may be used by anyone
 for any reason without any conditions and may be
 used as a starting point for your own applications
 which use HIDAPI.
********************************************************/

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#include "hidapi.h"

// Headers needed for sleeping.
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// Fallback/example
#ifndef HID_API_MAKE_VERSION
#define HID_API_MAKE_VERSION(mj, mn, p) (((mj) << 24) | ((mn) << 8) | (p))
#endif
#ifndef HID_API_VERSION
#define HID_API_VERSION HID_API_MAKE_VERSION(HID_API_VERSION_MAJOR, HID_API_VERSION_MINOR, HID_API_VERSION_PATCH)
#endif

// Sample using platform-specific headers
#if defined(__APPLE__) && HID_API_VERSION >= HID_API_MAKE_VERSION(0, 12, 0)
#include <hidapi_darwin.h>
#endif

#if defined(_WIN32) && HID_API_VERSION >= HID_API_MAKE_VERSION(0, 12, 0)
#include "hidapi_winapi.h"
#endif

#if defined(USING_HIDAPI_LIBUSB) && HID_API_VERSION >= HID_API_MAKE_VERSION(0, 12, 0)
#include <hidapi_libusb.h>
#endif

#define MAX_STR 255

enum REPORT_ID {
    PID_POOL_REPORT_ID = 0x13,
    PID_DEVICE_CONTROL_REPORT_ID = 0x0c,
    DEVICE_GAIN_REPORT_ID = 0x0d,
    CREATE_NEW_EFFECT_REPORT_ID = 0x11,
    PID_BLOCK_LOAD_REPORT_ID = 0x12,
    SET_CONSTANT_FORCE_REPORT_ID = 0x05,
    SET_ENVELOPE_REPORT_ID = 0x02,
    SET_EFFECT_REPORT_ID = 0x01,
    EFFECT_OPERATION_REPORT_ID = 0x0a,
} REPORT_ID;

const char* hid_bus_name(hid_bus_type bus_type) {
    static const char* const HidBusTypeName[] = {
            "Unknown",
            "USB",
            "Bluetooth",
            "I2C",
            "SPI",
    };

    if ((int)bus_type < 0)
        bus_type = HID_API_BUS_UNKNOWN;
    if ((int)bus_type >= (int)(sizeof(HidBusTypeName) / sizeof(HidBusTypeName[0])))
        bus_type = HID_API_BUS_UNKNOWN;

    return HidBusTypeName[bus_type];
}

void print_device(struct hid_device_info* cur_dev) {
    printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
    printf("\n");
    printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
    printf("  Product:      %ls\n", cur_dev->product_string);
    printf("  Release:      %hx\n", cur_dev->release_number);
    printf("  Interface:    %d\n", cur_dev->interface_number);
    printf("  Usage (page): 0x%hx (0x%hx)\n", cur_dev->usage, cur_dev->usage_page);
    printf("  Bus type: %u (%s)\n", (unsigned)cur_dev->bus_type, hid_bus_name(cur_dev->bus_type));
    printf("\n");
}

void print_hid_report_descriptor_from_device(hid_device* device) {
    unsigned char descriptor[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
    int res = 0;

    printf("  Report Descriptor: ");
    res = hid_get_report_descriptor(device, descriptor, sizeof(descriptor));
    if (res < 0) {
        printf("error getting: %ls", hid_error(device));
    }
    else {
        printf("(%d bytes)", res);
    }
    for (int i = 0; i < res; i++) {
        if (i % 10 == 0) {
            printf("\n");
        }
        printf("0x%02x, ", descriptor[i]);
    }
    printf("\n");
}

void print_hid_report_descriptor_from_path(const char* path) {
    hid_device* device = hid_open_path(path);
    if (device) {
        print_hid_report_descriptor_from_device(device);
        hid_close(device);
    }
    else {
        printf("  Report Descriptor: Unable to open device by path\n");
    }
}

void print_devices(struct hid_device_info* cur_dev) {
    for (; cur_dev; cur_dev = cur_dev->next) {
        print_device(cur_dev);
    }
}

void print_devices_with_descriptor(struct hid_device_info* cur_dev) {
    for (; cur_dev; cur_dev = cur_dev->next) {
        print_device(cur_dev);
        print_hid_report_descriptor_from_path(cur_dev->path);
    }
}

void read_once(hid_device* handle, unsigned char* buf, int len) {
    int res = 0;
    int i = 0;
    while (res == 0) {
        res = hid_read(handle, buf, sizeof(buf));
        if (res == 0) {
            printf("waiting...\n");
        }
        if (res < 0) {
            printf("Unable to read(): %ls\n", hid_error(handle));
            break;
        }

        i++;
        if (i >= 10) { /* 10 tries by 500 ms - 5 seconds of waiting*/
            printf("read() timeout\n");
            break;
        }

#ifdef _WIN32
        Sleep(500);
#else
        usleep(500 * 1000);
#endif
    }

    if (res > 0) {
        printf("Data read:\n   ");
        // Print out the returned buffer.
        for (i = 0; i < res; i++)
            printf("%02x ", (unsigned int)buf[i]);
        printf("\n");
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    int res;
    unsigned char buf[256];
    unsigned char index;
    wchar_t wstr[MAX_STR];
    hid_device* handle;
    int i;

    struct hid_device_info* devs;

    printf("hidapi test/example tool. Compiled with hidapi version %s, runtime version %s.\n", HID_API_VERSION_STR, hid_version_str());
    if (HID_API_VERSION == HID_API_MAKE_VERSION(hid_version()->major, hid_version()->minor, hid_version()->patch)) {
        printf("Compile-time version matches runtime version of hidapi.\n\n");
    }
    else {
        printf("Compile-time version is different than runtime version of hidapi.\n]n");
    }

    if (hid_init())
        return -1;

    handle = hid_open(0x346e, 0x0002, NULL);
    if (!handle) {
        printf("unable to open device\n");
        hid_exit();
        return 1;
    }

    // Copying the MOZA API calls hierarchy
    // 1. PID_POOL_REPORT
    // 2. PID_DEVICE_CONTROL_REPORT
    // 3. DEVICE_GAIN_REPORT
    // 4. CREATE_NEW_EFFECT_REPORT
    // 5. PID_BLOCK_LOAD_REPORT
    // 6. SET_CONSTANT_FORCE_REPORT
    // 7. SET_ENVELOPE_REPORT
    // 8. SET_EFFECT_REPORT
    // 9. EFFECT_OPERATION_REPORT
    // 6. SET_CONSTANT_FORCE_REPORT


    // 1. PID_POOL_REPORT
    // Endpoint: GET_REPORT
    memset(buf, 0x00, sizeof(buf));
    buf[0] = PID_POOL_REPORT_ID;

    res = hid_get_feature_report(handle, buf, 19);
    if (res < 0) {
        printf("Unable to get a feature report: %ls\n", hid_error(handle));
    }
    else {
        // Print out the returned buffer.
        printf("Feature Report\n   ");
        for (i = 0; i < res; i++)
            printf("%02x ", (unsigned int)buf[i]);
        printf("\n");
    }

    // 2. PID_DEVICE_CONTROL_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: 0b00001000 
    memset(buf, 0x00, sizeof(buf));
    buf[0] = PID_DEVICE_CONTROL_REPORT_ID;
    buf[1] = 0b00001000; // Device Control

    res = hid_write(handle, buf, 3);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 1\n");
    }

    // 3. DEVICE_GAIN_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: 0xff
    memset(buf, 0x00, sizeof(buf));
    buf[0] = DEVICE_GAIN_REPORT_ID;
    buf[1] = 0xff; // Device Gain

    res = hid_write(handle, buf, 3);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 2\n");
    }

    // 4. CREATE_NEW_EFFECT_REPORT
    // Endpoint: SET_REPORT
    // Data: 0x01
    memset(buf, 0x00, sizeof(buf));
    buf[0] = CREATE_NEW_EFFECT_REPORT_ID;
    buf[1] = 0x01; // Create New Effect

    res = hid_send_feature_report(handle, buf, 4);
    if (res < 0) {
        printf("Unable to send a feature report: %ls\n", hid_error(handle));
    }
    else {
        printf("Feature Report sent\n");
    }

    // 5. PID_BLOCK_LOAD_REPORT
    // Endpoint: GET_REPORT
    memset(buf, 0x00, sizeof(buf));
    buf[0] = PID_BLOCK_LOAD_REPORT_ID;

    res = hid_get_feature_report(handle, buf, 19);
    if (res < 0) {
        printf("Unable to get a feature report: %ls\n", hid_error(handle));
    }
    else {
        // Print out the returned buffer.
        printf("Feature Report\n   ");
        for (i = 0; i < res; i++)
            printf("%02x ", (unsigned int)buf[i]);
        printf("\n");
    }

    index = buf[1];

    // 6. SET_CONSTANT_FORCE_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: index (8), magnitude (16)
    memset(buf, 0x00, sizeof(buf));
    buf[0] = SET_CONSTANT_FORCE_REPORT_ID;
    buf[1] = index;
    buf[2] = 0xcd; // Magnitude
    buf[3] = 0x0c; // Magnitude

    res = hid_write(handle, buf, 5);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 3\n");
    }

    // 7. SET_ENVELOPE_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: index (8), attack level (16), fade level (16), attack time (32), fade time (32)
    memset(buf, 0x00, sizeof(buf));
    buf[0] = SET_ENVELOPE_REPORT_ID;
    buf[1] = index;
    buf[2] = 0x00; // Attack Level
    buf[3] = 0x00; // Attack Level
    buf[4] = 0x00; // Fade Level
    buf[5] = 0x00; // Fade Level
    buf[6] = 0x00; // Attack Time
    buf[7] = 0x00; // Attack Time
    buf[8] = 0x00; // Attack Time
    buf[9] = 0x00; // Attack Time
    buf[10] = 0x00; // Fade Time
    buf[11] = 0x00; // Fade Time
    buf[12] = 0x00; // Fade Time
    buf[13] = 0x00; // Fade Time

    res = hid_write(handle, buf, 15);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 4\n");
    }

    // 8. SET_EFFECT_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: 
    //      index (8), 
    //      effect type (8), 
    //      duration (16), 
    //      trigger repeat interval (16), 
    //      sample period (16)
    //      start delay (16)
    //      gain (8)
    //      trigger button (8)
    //      axe enable X (1), 
    //      axe enable Y (1),
    //      direction enable (1),
    //      padding (5),
    //      direction X (16), (en centi-degrees)
    //      direction Y (16),
    //	    type specific block offset 1 (16),
    //      type specific block offset 2 (16),
    memset(buf, 0x00, sizeof(buf));
    buf[0] = SET_EFFECT_REPORT_ID;
    buf[1] = index;
    buf[2] = 0x01; // Effect Type
    buf[3] = 0xff; // Duration 1
    buf[4] = 0xff; // Duration 2
    buf[5] = 0x00; // Trigger Repeat Interval 1
    buf[6] = 0x00; // Trigger Repeat Interval 2
    buf[7] = 0x00; // Sample Period 1
    buf[8] = 0x00; // Sample Period 2
    buf[9] = 0x00; // Start Delay 1
    buf[10] = 0x00; // Start Delay 2
    buf[11] = 0xff; // Gain
    buf[12] = 0xff; // Trigger Button
    buf[13] = 0b00000100; // Padding (5), Direction Enable (1), Axe Enable Y (1), Axe Enable X (1)
    buf[14] = 0x28; // Direction X 1 => 900.0 degrees
    buf[15] = 0x23; // Direction X 2 => 900.0 degrees 
    buf[16] = 0x00; // Direction Y 1
    buf[17] = 0x00; // Direction Y 2
    buf[18] = 0x00; // Type Specific Block Offset 1 1
    buf[19] = 0x00; // Type Specific Block Offset 1 2
    buf[20] = 0x00; // Type Specific Block Offset 2 1
    buf[21] = 0x00; // Type Specific Block Offset 2 2

    res = hid_write(handle, buf, 23);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 5\n");
    }

    // 9. EFFECT_OPERATION_REPORT
    // Endpoint: INTERRUPT_OUT
    // Data: 
    //      index (8),
    //      Op effect start (1),
    //      Op effect start solo (1),
    //      Op effect stop (1),
    //      Padding (5),
    //      Loop Count (8),
    memset(buf, 0x00, sizeof(buf));
    buf[0] = EFFECT_OPERATION_REPORT_ID;
    buf[1] = index;
    buf[2] = 0b00000001; // Padding (5), Op effect stop (1), Op effect start solo (1), Op effect start (1)

    res = hid_write(handle, buf, 4);
    if (res < 0) {
        printf("Unable to write(): %ls\n", hid_error(handle));
    }
    else {
        printf("Data written 6\n");
    }

    for (i = 0; i < 10; i++) {

        // SET_CONSTANT_FORCE_REPORT
        // Endpoint: INTERRUPT_OUT
        // Data: index (8), magnitude (16)
        memset(buf, 0x00, sizeof(buf));
        buf[0] = SET_CONSTANT_FORCE_REPORT_ID;
        buf[1] = index;
        buf[2] = 0xdc; // Magnitude
        buf[3] = 0x05; // Magnitude

        for (int i = 0; i < 100; i++) {
            res = hid_write(handle, buf, 5);
            Sleep(10);
        }

        // SET_CONSTANT_FORCE_REPORT
        // Endpoint: INTERRUPT_OUT
        // Data: index (8), magnitude (16)
        memset(buf, 0x00, sizeof(buf));
        buf[0] = SET_CONSTANT_FORCE_REPORT_ID;
        buf[1] = index;
        buf[2] = 0x24; // Magnitude
        buf[3] = 0xfa; // Magnitude

        for (int i = 0; i < 100; i++) {
            res = hid_write(handle, buf, 5);
            Sleep(10);
        }

    }

    memset(buf, 0x00, sizeof(buf));
    buf[0] = SET_CONSTANT_FORCE_REPORT_ID;
    buf[1] = index;
    buf[2] = 0x00; // Magnitude
    buf[3] = 0x00; // Magnitude

    for (int i = 0; i < 100; i++) {
        res = hid_write(handle, buf, 5);
        Sleep(10);
    }

    hid_close(handle);

    /* Free static HIDAPI objects. */
    hid_exit();

#ifdef _WIN32
    system("pause");
#endif

    return 0;
}