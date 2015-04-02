//////////////////////////////////////////////////////////////////////////////////
// ftl.h for Cosmos OpenSSD
// Copyright (c) 2014 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//                Gyeongyong Lee <gylee@enc.hanyang.ac.kr>
//
// This file is part of Cosmos OpenSSD.
//
// Cosmos OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Gyeongyong Lee <gylee@enc.hanyang.ac.kr>
//
// Project Name: Cosmos OpenSSD
// Design Name: Tutorial FTL
// Module Name: Flash Translation Layer
// File Name: ftl.h
//
// Version: v1.0.0
//
// Description:
//   - define NAND flash and SSD parameters
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#ifndef	FTL_H_
#define	FTL_H_

// bitwise representation
#define	SECTOR_SIZE_B			9

#define	PAGE_SIZE_B				13	// 8KB
#define	PAGE_NUM_PER_BLOCK_B	8	// 256
#define	BLOCK_NUM_PER_DIE_B		12	// 4096

#define	CHANNEL_NUM_B			2
#define	WAY_NUM_B				2
#define	DIE_NUM_B				(CHANNEL_NUM_B + WAY_NUM_B)

#define	SECTOR_NUM_PER_PAGE_B	(PAGE_SIZE_B - SECTOR_SIZE_B)

#define	PAGE_NUM_PER_DIE_B		(PAGE_NUM_PER_BLOCK_B + BLOCK_NUM_PER_DIE_B)
#define	PAGE_NUM_PER_CHANNEL_B	(PAGE_NUM_PER_DIE_B + WAY_NUM_B)
#define	PAGE_NUM_PER_SSD_B		(PAGE_NUM_PER_CHANNEL_B + CHANNEL_NUM_B)

#define	BLOCK_NUM_PER_CHANNEL_B	(BLOCK_NUM_PER_DIE_B + WAY_NUM_B)
#define	BLOCK_NUM_PER_SSD_B		(BLOCK_NUM_PER_CHANNEL_B + CHANNEL_NUM_B)

// shift bitwise representation
#define	PAGE_SIZE				(0x1 << PAGE_SIZE_B)
#define	PAGE_NUM_PER_BLOCK		(0x1 << PAGE_NUM_PER_BLOCK_B)
#define	BLOCK_NUM_PER_DIE		(0x1 << BLOCK_NUM_PER_DIE_B)

#define	CHANNEL_NUM				(0x1 << CHANNEL_NUM_B)
#define	WAY_NUM					(0x1 << WAY_NUM_B)
#define	DIE_NUM					(0x1 << DIE_NUM_B)

#define	SECTOR_NUM_PER_PAGE		(0x1 << SECTOR_NUM_PER_PAGE_B)

#define	PAGE_NUM_PER_DIE		(0x1 << PAGE_NUM_PER_DIE_B)
#define	PAGE_NUM_PER_CHANNEL	(0x1 << PAGE_NUM_PER_CHANNEL_B)
#define	PAGE_NUM_PER_SSD		(0x1 << PAGE_NUM_PER_SSD_B)

#define	BLOCK_NUM_PER_CHANNEL	(0x1 << BLOCK_NUM_PER_CHANNEL_B)
#define	BLOCK_NUM_PER_SSD		(0x1 << BLOCK_NUM_PER_SSD_B)

// bitwise modulo operation
#define	MODULAR(X)				((0x1 << (X)) - 1)

void InitNandReset();
void InitFtlMapTable();

#endif /* FTL_H_ */
