//////////////////////////////////////////////////////////////////////////////////
// page_map.c for Cosmos OpenSSD
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
// Design Name: Greedy FTL
// Module Name: Page Mapping
// File Name: page_map.c
//
// Version: v2.0.0
//
// Description:
//   - initialize map tables
//   - read/write request
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v2.0.0
//   - add garbage collection
//
// * v1.1.0
//   - support static bad block management
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "pagemap.h"

#include <assert.h>

#include "lld.h"

void InitPageMap()
{
	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);

//	xil_printf("PAGE_MAP_ADDR : %8x\r\n", PAGE_MAP_ADDR);

	// page status initialization, allows lpn, ppn access
	int i, j;
	for(i=0 ; i<DIE_NUM ; i++)
	{
		for(j=0 ; j<PAGE_NUM_PER_DIE ; j++)
		{
			pageMap->pmEntry[i][j].ppn = 0xffffffff;

			pageMap->pmEntry[i][j].valid = 1;
			pageMap->pmEntry[i][j].lpn = 0x7fffffff;
		}
	}

	xil_printf("[ ssd page map initialized. ]\r\n");
}

void InitBlockMap()
{
	blockMap = (struct bmArray*)(BLOCK_MAP_ADDR);

//	xil_printf("BLOCK_MAP_ADDR : %8x\r\n", BLOCK_MAP_ADDR);

	// block status initialization, allows only physical access
	int i, j;
	for(i=0 ; i<BLOCK_NUM_PER_DIE ; i++)
	{
		for(j=0 ; j<DIE_NUM ; j++)
		{
			blockMap->bmEntry[j][i].bad = 0;
			blockMap->bmEntry[j][i].free = 1;
			blockMap->bmEntry[j][i].eraseCnt = 0;
			blockMap->bmEntry[j][i].invalidPageCnt = 0;
			blockMap->bmEntry[j][i].currentPage = 0x0;
			blockMap->bmEntry[j][i].prevBlock = 0xffffffff;
			blockMap->bmEntry[j][i].nextBlock = 0xffffffff;
		}
	}

	// static bad block management
	// mark bad blocks by bad flag
	for (i = 0; i < DIE_NUM; ++i)
	{
		blockMap->bmEntry[i][90].bad = 1;	// mark 90-block of all dies as bad block
		blockMap->bmEntry[i][91].bad = 1;	// mark 91-block of all dies as bad block
	}

	u32 dieNo = 0 + 3*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][820].bad = 1;	// mark 0-ch, 3-way, 820-block as bad block
	dieNo = 3 + 2*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][1050].bad = 1;	// mark 3-ch, 2-way, 1050-block as bad block
	dieNo = 2 + 1*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][1799].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][2206].bad = 1;
	dieNo = 1 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][2945].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3064].bad = 1;
	dieNo = 2 + 1*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3121].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3176].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3178].bad = 1;
	dieNo = 3 + 2*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3228].bad = 1;
	dieNo = 1 + 3*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3237].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3368].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][3774].bad = 1;
	dieNo = 2 + 0*CHANNEL_NUM;
	blockMap->bmEntry[dieNo][4016].bad = 1;	// mark 2-ch, 0-way, 4016-block as bad block

	for (i = 0; i < BLOCK_NUM_PER_DIE; ++i)
		for (j = 0; j < DIE_NUM; ++j)
			if (!blockMap->bmEntry[j][i].bad)
			{
				// initial block erase
				WaitWayFree(j % CHANNEL_NUM, j / CHANNEL_NUM);
				SsdErase(j % CHANNEL_NUM, j / CHANNEL_NUM, i);
			}

	xil_printf("[ ssd entire block erasure completed. ]\r\n");

	for(i=0 ; i<DIE_NUM ; i++)
	{
		// initially, 0th block of each die is allocated for storage start point
		blockMap->bmEntry[i][0].free = 0;
		blockMap->bmEntry[i][0].currentPage = 0xffff;

		// initially, the last block of each die is reserved as free block for GC migration
		blockMap->bmEntry[i][BLOCK_NUM_PER_DIE-1].free = 0;
	}

	xil_printf("[ ssd block map initialized. ]\r\n");
}

void InitDieBlock()
{
	dieBlock = (struct dieArray*)(DIE_MAP_ADDR);

//	xil_printf("DIE_MAP_ADDR : %8x\r\n", DIE_MAP_ADDR);

	int i;
	for(i=0 ; i<DIE_NUM ; i++)
	{
		dieBlock->dieEntry[i].currentBlock = 0;
		dieBlock->dieEntry[i].freeBlock = BLOCK_NUM_PER_DIE - 1;
	}

	xil_printf("[ ssd die map initialized. ]\r\n");
}

void InitGcMap()
{
	gcMap = (struct gcArray*)(GC_MAP_ADDR);

//	xil_printf("GC_MAP_ADDR : %8x\r\n", GC_MAP_ADDR);

	// gc table status initialization
	int i, j;
	for(i=0 ; i<DIE_NUM ; i++)
	{
		for(j=0 ; j<PAGE_NUM_PER_BLOCK+1 ; j++)
		{
			gcMap->gcEntry[i][j].head = 0xffffffff;
			gcMap->gcEntry[i][j].tail = 0xffffffff;
		}
	}

	xil_printf("[ ssd gc map initialized. ]\r\n");
}

int FindFreePage(u32 dieNo)
{
	blockMap = (struct bmArray*)(BLOCK_MAP_ADDR);
	dieBlock = (struct dieArray*)(DIE_MAP_ADDR);

	if(blockMap->bmEntry[dieNo][dieBlock->dieEntry[dieNo].currentBlock].currentPage == PAGE_NUM_PER_BLOCK-1)
	{
		dieBlock->dieEntry[dieNo].currentBlock++;

		int i;
		for(i=dieBlock->dieEntry[dieNo].currentBlock ; i<(dieBlock->dieEntry[dieNo].currentBlock + BLOCK_NUM_PER_DIE) ; i++)
		{
			if((blockMap->bmEntry[dieNo][i % BLOCK_NUM_PER_DIE].free) && (!blockMap->bmEntry[dieNo][i % BLOCK_NUM_PER_DIE].bad))
			{
				blockMap->bmEntry[dieNo][i % BLOCK_NUM_PER_DIE].free = 0;
				dieBlock->dieEntry[dieNo].currentBlock = i % BLOCK_NUM_PER_DIE;

//				xil_printf("allocated free block: %4d at %d-%d\r\n", dieBlock->dieEntry[dieNo].currentBlock, dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);

				return dieBlock->dieEntry[dieNo].currentBlock * PAGE_NUM_PER_BLOCK;
			}
		}

		dieBlock->dieEntry[dieNo].currentBlock = GarbageCollection(dieNo);

//		xil_printf("allocated free block by GC: %4d at %d-%d\r\n", dieBlock->dieEntry[dieNo].currentBlock, dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);

		return (dieBlock->dieEntry[dieNo].currentBlock * PAGE_NUM_PER_BLOCK) + blockMap->bmEntry[dieNo][dieBlock->dieEntry[dieNo].currentBlock].currentPage;
	}
	else
	{
		blockMap->bmEntry[dieNo][dieBlock->dieEntry[dieNo].currentBlock].currentPage++;
		return (dieBlock->dieEntry[dieNo].currentBlock * PAGE_NUM_PER_BLOCK) + blockMap->bmEntry[dieNo][dieBlock->dieEntry[dieNo].currentBlock].currentPage;
	}
}

int PrePmRead(P_HOST_CMD hostCmd, u32 bufferAddr)
{
	u32 lpn;
	u32 dieNo;
	u32 dieLpn;

	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);

	if((((hostCmd->reqInfo.currentSect)&MODULAR(SECTOR_NUM_PER_PAGE_B)) != 0)
			|| ((hostCmd->reqInfo.currentSect / SECTOR_NUM_PER_PAGE) == (((hostCmd->reqInfo.currentSect)+(hostCmd->reqInfo.ReqSect))>>SECTOR_NUM_PER_PAGE_B)))
	{
		lpn = hostCmd->reqInfo.currentSect / SECTOR_NUM_PER_PAGE;
		dieNo = lpn % DIE_NUM;
		dieLpn = lpn / DIE_NUM;

		if(pageMap->pmEntry[dieNo][dieLpn].ppn != 0xffffffff)
		{

//			xil_printf("PrePmRead pdie, ppn = %d, %d\r\n", dieNo, pageMap->pmEntry[dieNo][dieLpn].ppn);

			WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
			SsdRead(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, pageMap->pmEntry[dieNo][dieLpn].ppn, bufferAddr);
			WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
		}
	}

	if(((((hostCmd->reqInfo.currentSect)+(hostCmd->reqInfo.ReqSect))&MODULAR(SECTOR_NUM_PER_PAGE_B)) != 0)
			&& ((hostCmd->reqInfo.currentSect / SECTOR_NUM_PER_PAGE) != (((hostCmd->reqInfo.currentSect)+(hostCmd->reqInfo.ReqSect))>>SECTOR_NUM_PER_PAGE_B)))
	{
		lpn = ((hostCmd->reqInfo.currentSect)+(hostCmd->reqInfo.ReqSect))>>SECTOR_NUM_PER_PAGE_B;
		dieNo = lpn % DIE_NUM;
		dieLpn = lpn / DIE_NUM;

		if(pageMap->pmEntry[dieNo][dieLpn].ppn != 0xffffffff)
		{

//			xil_printf("PrePmRead pdie, ppn = %d, %d\r\n", dieNo, pageMap->pmEntry[dieNo][dieLpn].ppn);

			WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
			SsdRead(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, pageMap->pmEntry[dieNo][dieLpn].ppn,
					bufferAddr + ((((hostCmd->reqInfo.currentSect)&MODULAR(SECTOR_NUM_PER_PAGE_B)) + hostCmd->reqInfo.ReqSect)>>SECTOR_NUM_PER_PAGE_B<<PAGE_SIZE_B));
			WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
		}
	}

	return 0;
}

int PmRead(P_HOST_CMD hostCmd, u32 bufferAddr)
{
	u32 tempBuffer = bufferAddr;
	
	u32 lpn = hostCmd->reqInfo.currentSect / SECTOR_NUM_PER_PAGE;
	int loop = (hostCmd->reqInfo.currentSect % SECTOR_NUM_PER_PAGE) + hostCmd->reqInfo.ReqSect;
	
	u32 dieNo;
	u32 dieLpn;

	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);

	while(loop > 0)
	{
		dieNo = lpn % DIE_NUM;
		dieLpn = lpn / DIE_NUM;

//		xil_printf("requested read lpn = %d\r\n", lpn);
//		xil_printf("read pdie, ppn = %d, %d\r\n", dieNo, pageMap->pmEntry[dieNo][dieLpn].ppn);

		if(pageMap->pmEntry[dieNo][dieLpn].ppn != 0xffffffff)
		{
//			xil_printf("read at (%d, %2d, %4x)\r\n", dieNo%CHANNEL_NUM, dieNo/CHANNEL_NUM, pageMap->pmEntry[dieNo][dieLpn].ppn);

			WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
			SsdRead(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, pageMap->pmEntry[dieNo][dieLpn].ppn, tempBuffer);
		}

		lpn++;
		tempBuffer += PAGE_SIZE;
		loop -= SECTOR_NUM_PER_PAGE;
	}

	int i;
	for(i=0 ; i<DIE_NUM ; ++i)
		WaitWayFree(i%CHANNEL_NUM, i/CHANNEL_NUM);

	return 0;
}

int PmWrite(P_HOST_CMD hostCmd, u32 bufferAddr)
{
	u32 tempBuffer = bufferAddr;
	
	u32 lpn = hostCmd->reqInfo.currentSect / SECTOR_NUM_PER_PAGE;
	
	int loop = (hostCmd->reqInfo.currentSect % SECTOR_NUM_PER_PAGE) + hostCmd->reqInfo.ReqSect;
	
	u32 dieNo;
	u32 dieLpn;
	u32 freePageNo;

	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);
	blockMap = (struct bmArray*)(BLOCK_MAP_ADDR);
	gcMap = (struct gcArray*)(GC_MAP_ADDR);

	while(loop > 0)
	{
		dieNo = lpn % DIE_NUM;
		dieLpn = lpn / DIE_NUM;
		freePageNo = FindFreePage(dieNo);

//		xil_printf("free page: %6d(%d, %d, %4d)\r\n", freePageNo, dieNo%CHANNEL_NUM, dieNo/CHANNEL_NUM, freePageNo/PAGE_NUM_PER_BLOCK);

		WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
		SsdProgram(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, freePageNo, tempBuffer);
		
		if(pageMap->pmEntry[dieNo][dieLpn].ppn != 0xffffffff)
		{
//			xil_printf("overwrite occurs!\r\n");

			// GC victim block list management
			u32 diePbn = pageMap->pmEntry[dieNo][dieLpn].ppn / PAGE_NUM_PER_BLOCK;

			// unlink
			if((blockMap->bmEntry[dieNo][diePbn].nextBlock != 0xffffffff) && (blockMap->bmEntry[dieNo][diePbn].prevBlock != 0xffffffff))
			{
				blockMap->bmEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].prevBlock].nextBlock = blockMap->bmEntry[dieNo][diePbn].nextBlock;
				blockMap->bmEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].nextBlock].prevBlock = blockMap->bmEntry[dieNo][diePbn].prevBlock;
			}
			else if((blockMap->bmEntry[dieNo][diePbn].nextBlock == 0xffffffff) && (blockMap->bmEntry[dieNo][diePbn].prevBlock != 0xffffffff))
			{
				blockMap->bmEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].prevBlock].nextBlock = 0xffffffff;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail = blockMap->bmEntry[dieNo][diePbn].prevBlock;
			}
			else if((blockMap->bmEntry[dieNo][diePbn].nextBlock != 0xffffffff) && (blockMap->bmEntry[dieNo][diePbn].prevBlock == 0xffffffff))
			{
				blockMap->bmEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].nextBlock].prevBlock = 0xffffffff;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].head = blockMap->bmEntry[dieNo][diePbn].nextBlock;
			}
			else
			{
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].head = 0xffffffff;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail = 0xffffffff;
			}

//			xil_printf("[unlink] dieNo = %d, invalidPageCnt= %d, diePbn= %d, blockMap.prevBlock= %d, blockMap.nextBlock= %d, gcMap.head= %d, gcMap.tail= %d\r\n", dieNo, blockMap->bmEntry[dieNo][diePbn].invalidPageCnt, diePbn, blockMap->bmEntry[dieNo][diePbn].prevBlock, blockMap->bmEntry[dieNo][diePbn].nextBlock, gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].head, gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail);

			// invalidation update
			pageMap->pmEntry[dieNo][pageMap->pmEntry[dieNo][dieLpn].ppn].valid = 0;
			blockMap->bmEntry[dieNo][diePbn].invalidPageCnt++;

			// insertion
			if(gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail != 0xffffffff)
			{
				blockMap->bmEntry[dieNo][diePbn].prevBlock = gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail;
				blockMap->bmEntry[dieNo][diePbn].nextBlock = 0xffffffff;
				blockMap->bmEntry[dieNo][gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail].nextBlock = diePbn;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail = diePbn;
			}
			else
			{
				blockMap->bmEntry[dieNo][diePbn].prevBlock = 0xffffffff;
				blockMap->bmEntry[dieNo][diePbn].nextBlock = 0xffffffff;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].head = diePbn;
				gcMap->gcEntry[dieNo][blockMap->bmEntry[dieNo][diePbn].invalidPageCnt].tail = diePbn;
			}
		}
		
		// pageMap update
		pageMap->pmEntry[dieNo][dieLpn].ppn = freePageNo;
		pageMap->pmEntry[dieNo][freePageNo].lpn = dieLpn;

		lpn++;
		tempBuffer += PAGE_SIZE;
		loop -= SECTOR_NUM_PER_PAGE;
	}

	int i;
	for(i=0 ; i<DIE_NUM ; ++i)
		WaitWayFree(i%CHANNEL_NUM, i/CHANNEL_NUM);

	return 0;
}

void EraseBlock(u32 dieNo, u32 blockNo)
{
	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);
	blockMap = (struct bmArray*)(BLOCK_MAP_ADDR);

	// block map indicated blockNo initialization
	blockMap->bmEntry[dieNo][blockNo].free = 1;
	blockMap->bmEntry[dieNo][blockNo].eraseCnt++;
	blockMap->bmEntry[dieNo][blockNo].invalidPageCnt = 0;
	blockMap->bmEntry[dieNo][blockNo].currentPage = 0x0;
	blockMap->bmEntry[dieNo][blockNo].prevBlock = 0xffffffff;
	blockMap->bmEntry[dieNo][blockNo].nextBlock = 0xffffffff;

	int i;
	for(i=0 ; i<PAGE_NUM_PER_BLOCK ; i++)
	{
		pageMap->pmEntry[dieNo][(blockNo * PAGE_NUM_PER_BLOCK) + i].valid = 1;
		pageMap->pmEntry[dieNo][(blockNo * PAGE_NUM_PER_BLOCK) + i].lpn = 0x7fffffff;
	}

	WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
	SsdErase(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, blockNo);
}

u32 GarbageCollection(u32 dieNo)
{
//	xil_printf("GC occurs!\r\n");

	pageMap = (struct pmArray*)(PAGE_MAP_ADDR);
	blockMap = (struct bmArray*)(BLOCK_MAP_ADDR);
	dieBlock = (struct dieArray*)(DIE_MAP_ADDR);
	gcMap = (struct gcArray*)(GC_MAP_ADDR);

	int i;
	for(i=PAGE_NUM_PER_BLOCK ; i>0 ; i--)
	{
		if(gcMap->gcEntry[dieNo][i].head != 0xffffffff)
		{
			u32 victimBlock = gcMap->gcEntry[dieNo][i].head;	// GC victim block

			// link setting
			if(blockMap->bmEntry[dieNo][victimBlock].nextBlock != 0xffffffff)
			{
				gcMap->gcEntry[dieNo][i].head = blockMap->bmEntry[dieNo][victimBlock].nextBlock;
				blockMap->bmEntry[dieNo][blockMap->bmEntry[dieNo][victimBlock].nextBlock].prevBlock = 0xffffffff;
			}
			else
			{
				gcMap->gcEntry[dieNo][i].head = 0xffffffff;
				gcMap->gcEntry[dieNo][i].tail = 0xffffffff;
			}

			// copy valid pages from the victim block to the free block
			if(i != PAGE_NUM_PER_BLOCK)
			{
				int j;
				for(j=0 ; j<PAGE_NUM_PER_BLOCK ; j++)
				{
					if(pageMap->pmEntry[dieNo][(victimBlock * PAGE_NUM_PER_BLOCK) + j].valid)
					{
						// page copy process
						u32 validPage = victimBlock*PAGE_NUM_PER_BLOCK + j;
						u32 freeBlock = dieBlock->dieEntry[dieNo].freeBlock;
						u32 freePage = freeBlock*PAGE_NUM_PER_BLOCK + blockMap->bmEntry[dieNo][freeBlock].currentPage;

						WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
						SsdRead(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, validPage, GC_BUFFER_ADDR);
						WaitWayFree(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM);
						SsdProgram(dieNo % CHANNEL_NUM, dieNo / CHANNEL_NUM, freePage, GC_BUFFER_ADDR);

						// pageMap, blockMap update
						u32 lpn = pageMap->pmEntry[dieNo][validPage].lpn;

						pageMap->pmEntry[dieNo][lpn].ppn = freePage;
						pageMap->pmEntry[dieNo][freePage].lpn = lpn;
						blockMap->bmEntry[dieNo][freeBlock].currentPage++;
					}
				}
			}

			// erased victim block becomes the free block for GC migration
			EraseBlock(dieNo, victimBlock);
			blockMap->bmEntry[dieNo][victimBlock].free = 0;

			u32 currentBlock = dieBlock->dieEntry[dieNo].freeBlock;
			dieBlock->dieEntry[dieNo].freeBlock = victimBlock;

			return currentBlock;	// atomic GC completion
		}
	}

	// no free space anymore
	assert(!"[WARNING] There are no free blocks. Abort terminate this ssd. [WARNING]");
	return 1;
}
