/* -*- mode: c; tab-width: 4; c-basic-offset: 4 -*- */
/*
 * features.h
 *
 *  Created on: 2011-02-11
 *      Author: Roger E Critchlow Jr, AD5DZ
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "flashc.h"

#include "features.h"
#include "widget.h"

// Set up NVRAM (EEPROM) storage
#if defined (__GNUC__)
__attribute__((__section__(".userpage")))
#endif
features_t features_nvram;

features_t features = { FEATURES_DEFAULT };

//
// these arrays of names need to be kept in sync
// with the enumerations defined in features.h
//
const char *feature_value_names[] = { FEATURE_VALUE_NAMES };

const char *feature_index_names[] = { FEATURE_INDEX_NAMES };

//
void features_init() {
  // Enforce "Factory default settings" when a mismatch is detected between the
  // checksum in the memory copy and the matching number in the NVRAM storage.
  // This can be the result of either a fresh firmware upload, or cmd 0x41 with data 0xff
  if( FEATURE_MAJOR != FEATURE_MAJOR_NVRAM || FEATURE_MINOR != FEATURE_MINOR_NVRAM ) {
	  flashc_memcpy((void *)&features_nvram, &features, sizeof(features), TRUE);
  } else {
	  memcpy(&features, &features_nvram, sizeof(features));
  }
}

void features_display(char *title, features_t fp, int delay) {
	int i;
	char buff[32];
	widget_display_string_scroll_and_delay(title, delay);
	sprintf(buff, "%s = %u.%u", "version", fp[feature_major_index], fp[feature_minor_index]);
	widget_display_string_scroll_and_delay(buff, delay);
	for (i = feature_board_index; i < feature_end_index; i += 1) {
		strcpy(buff, feature_index_names[i]);
		strcat(buff, " = ");
		if (features[i] < feature_end_values)
			strcat(buff, (char *)feature_value_names[fp[i]]);
		else
			strcat(buff, "invalid!");
		widget_display_string_scroll_and_delay(buff, delay);
	}
}

void features_display_all() {
	widget_display_clear();
	widget_report();
	features_display("features ram:", features, 10000);
	// features_display("features nvram:", features_nvram, 10000);
}

uint8_t feature_set(uint8_t index, uint8_t value) {
	return index > feature_minor_index && index < feature_end_index && value < feature_end_values ?
		features[index] = value :
		0xFF;
}

uint8_t feature_get(uint8_t index) {
	return index < feature_end_index ? features[index] : 0xFF;
}

uint8_t feature_set_nvram(uint8_t index, uint8_t value)  {
	if ( index > feature_minor_index && index < feature_end_index && value < feature_end_values ) {
		flashc_memset8((void *)&features_nvram[index], value, sizeof(uint8_t), TRUE);
		return features_nvram[index];
	} else
		return 0xFF;
}

uint8_t feature_get_nvram(uint8_t index)  {
	return index < feature_end_index ? features_nvram[index] : 0xFF;
}
