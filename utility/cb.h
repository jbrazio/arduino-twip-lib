/*
 * cb.h - Circular Buffer library
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

#ifndef __cb_h____
#define __cb_h____

#include <Arduino.h>

class cb {
	private:
		uint8_t  cb_size;
		uint8_t  cb_start;
		uint8_t  cb_end;
		uint8_t* cb_buffer;

	public:
		~cb( void );
		uint8_t read( void );
		uint8_t empty( void );
		uint8_t available( void );
		uint8_t init( uint8_t size );
		uint8_t write( uint8_t byte );
		uint8_t peek( uint8_t offset );
};

#endif