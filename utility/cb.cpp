/*
 * cb.cpp - Circular Buffer library
 * Copyright (c) 2012 João Brázio <joao@brazio.org>, all rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>
#include "cb.h"

/*
 * Function: class constructor
 *    Input: No input.
 *   Output: No output.
 *
 * Description: When destroying the class deallocate the memory.
 *
 */
cb::~cb( void ) { free( this->cb_buffer ); }

/*
 * Function: cb::init
 *    Input: uint8_t size represents to total size of the circular buffer,
 *   Output: No output.
 *
 * Description: The maximum buffer size is 254 because internally one byte is used for accounting.
 * This function will silently refuse to accept any uint8_t size value higer then 254.
 *
 */
uint8_t cb::init( uint8_t size ) {
	if( size > 254 ) { size = 254; }

	this->cb_start  = 0;	// Read pointer
	this->cb_end    = 0;	// Write pointer
	this->cb_size   = size +1;
	this->cb_buffer = (uint8_t *) calloc( this->cb_size, sizeof(uint8_t) );
}

/*
 * Function: cb::read
 *    Input: No input.
 *   Output: Returns the first byte in the buffer.
 *
 * Description: Reads the oldest byte on buffer, removes it from buffer and return it to caller.
 *
 */
uint8_t cb::read( void ) {
	uint8_t ret = this->cb_buffer[ this->cb_start ];
	this->cb_start = (this->cb_start +1) % this->cb_size;
	return ret;
}

/*
 * Function: cb::write
 *    Input: uint8_t byte to be written into the buffer.
 *   Output: Boolean representing: 1 - Success, 0 - Failure.
 *
 * Description: Write a byte into the end of buffer.
 *
 */
uint8_t cb::write( uint8_t byte ) {
	// Refuse to overwrite data if buffer is full
	if( ((this->cb_end +1) % this->cb_size) == this->cb_start ) { return false; }

	// Write data to buffer and update end pointer
	this->cb_buffer[ this->cb_end ] = byte;
	this->cb_end = (this->cb_end +1) % this->cb_size;

	return true;
}

/*
 * Function: cb::peek
 *    Input: uint8_t offset is the number of bytes to skip
 *   Output: Returns the n byte in the buffer.
 *
 * Description: This function is very similiar to cb::read with the exception that the read byte will
 * not be removed from buffer thus the name "peek".
 *
 */
uint8_t cb::peek( uint8_t offset ) {
	return this->cb_buffer[ (this->cb_start + offset) % this->cb_size ];
}

/*
 * Function: cb::empty
 *    Input: No input.
 *   Output: Boolean representing: 1 - Buffer is empty, 0 - Buffer is not empty.
 *
 * Description: No description.
 *
 */
uint8_t cb::empty( void ) {
	return (this->cb_start == this->cb_end) ? true : false;
}

/*
 * Function: cb::available
 *    Input: No input.
 *   Output: No output.
 *
 * Description: Boolean representing: 1 - At least one byte at buffer, 0 - No packets at buffer.
 *
 */
uint8_t cb::available( void ) {
	return ( (this->cb_size -1) - (this->cb_end - this->cb_start) );
}