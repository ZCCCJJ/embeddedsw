/******************************************************************************
*
* Copyright (C) 2015 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file xfsbl_image_header.c
*
* This is the image header C file which does validation for the image header.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00  kc   10/21/13 Initial release
*
* </pre>
*
* @note
*
******************************************************************************/


/***************************** Include Files *********************************/
#include "xfsbl_hw.h"
#include "xfsbl_image_header.h"
#include "xfsbl_misc_drivers.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/
__inline u32 XFsbl_GetPartitionOwner(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_PART_OWNER_MASK;

}

__inline u32 XFsbl_IsRsaSignaturePresent(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return  PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_RSA_SIGNATURE_MASK;
}

__inline u32 XFsbl_GetChecksumType(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_CHECKSUM_MASK;
}

__inline u32 XFsbl_GetDestinationCpu(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_DEST_CPU_MASK;
}


__inline u32 XFsbl_IsEncrypted(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_ENCRYPTION_MASK;
}

__inline u32 XFsbl_GetDestinationDevice(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_DEST_DEVICE_MASK;
}

__inline u32 XFsbl_GetA53ExecState(
                XFsblPs_PartitionHeader * PartitionHeader)
{
        return PartitionHeader->PartitionAttributes &
                                XIH_PH_ATTRB_A53_EXEC_ST_MASK;
}

/************************** Function Prototypes ******************************/
static u32 XFsbl_ValidateImageHeaderTable(
		XFsblPs_ImageHeaderTable * ImageHeaderTable);
static u32 XFsbl_CheckValidMemoryAddress(u64 Address, u32 CpuId, u32 DevId);
static void XFsbl_SetATFHandoffParameters(
		XFsblPs_PartitionHeader *PartitionHeader, u32 EntryCount);

/************************** Variable Definitions *****************************/
XFsblPs_ATFHandoffParams ATFHandoffParams;

/****************************************************************************/
/**
*  This function is used to validate the word checksum for the image header
*  table and partition headers.
*  Checksum is based on the below formulae
*  	Checksum = ~(X1 + X2 + X3 + .... + Xn)
*
* @param Buffer pointer for the data words.
*
* @param Length of the buffer for which checksum should be calculated.
* last word is taken as checksum for the data to be compared against
*
* @return
* 		- XFSBL_SUCCESS for successful checksum validation
* 		- XFSBL_FAILURE if checksum validation fails
*
* @note
*
*****************************************************************************/
u32 XFsbl_ValidateChecksum(u32 Buffer[], u32 Length)
{
	u32 Status=XFSBL_SUCCESS;
	u32 Checksum=0U;
	u32 Count=0U;

	/**
	 * Length has to be atleast equal to 2,
	 */
	if (Length < 2U)
	{
		Status = XFSBL_FAILURE;
		goto END;
	}

	/**
	 * Checksum = ~(X1 + X2 + X3 + .... + Xn)
	 * Calculate the checksum
	 */
	for (Count = 0U; Count < (Length-1U); Count++) {
		/**
		 * Read the word from the header
		 */
		Checksum += Buffer[Count];
	}

	/**
	 * Invert checksum
	 */
	Checksum ^= 0xFFFFFFFFU;

	/**
	 * Validate the checksum
	 */
	if (Buffer[Length-1U] != Checksum) {
		XFsbl_Printf(DEBUG_GENERAL,
		    "Error: Checksum 0x%0lx != %0lx\r\n",
			Checksum, Buffer[Length-1U]);
		Status =  XFSBL_FAILURE;
	}

END:
	return Status;
}

/****************************************************************************/
/**
*  This function checks the fields of the image header table and validates
*  them. Image header table contains the fields that common across the
*  partitions and for image
*
* @param ImageHeaderTable pointer to the image header table.
*
* @return
* 	- XFSBL_SUCCESS on successful image header table validation
* 	- errors as mentioned in xfsbl_error.h
*
* @note
*
*****************************************************************************/

static u32 XFsbl_ValidateImageHeaderTable(
		XFsblPs_ImageHeaderTable * ImageHeaderTable)
{
	u32 Status = XFSBL_SUCCESS;
	u32 PartitionPresentDevice=0U;

	/**
	 * Check the check sum of the image header table
	 */
	Status = XFsbl_ValidateChecksum((u32 *)ImageHeaderTable,
				XIH_IHT_LEN/XIH_PARTITION_WORD_LENGTH);
	if (XFSBL_SUCCESS != Status)
	{
		Status = XFSBL_ERROR_IHT_CHECKSUM;
		XFsbl_Printf(DEBUG_GENERAL,"XFSBL_ERROR_IHT_CHECKSUM\n\r");
		goto END;
	}

	/**
	 * check for no of partitions
	 */
	if ((ImageHeaderTable->NoOfPartitions <= XIH_MIN_PARTITIONS ) ||
		(ImageHeaderTable->NoOfPartitions > XIH_MAX_PARTITIONS) )
	{
		Status = XFSBL_ERROR_NO_OF_PARTITIONS;
		XFsbl_Printf(DEBUG_GENERAL,"XFSBL_ERROR_NO_OF_PARTITIONS\n\r");
		goto END;
	}

	/**
	 * check for the partition present device
	 */
	PartitionPresentDevice = ImageHeaderTable->PartitionPresentDevice;
	if ((PartitionPresentDevice != XIH_IHT_PPD_SAME) &&
		(PartitionPresentDevice != XIH_IHT_PPD_PCIE) &&
		(PartitionPresentDevice != XIH_IHT_PPD_ETHERNET) &&
		(PartitionPresentDevice != XIH_IHT_PPD_SATA) )
	{
		Status = XFSBL_ERROR_PPD;
		XFsbl_Printf(DEBUG_GENERAL,
		    "XFSBL_ERROR_PPD\n\r");
		goto END;
	}


	/**
	 * Print the Image header table details
	 * Print the Bootgen version
	 */
	XFsbl_Printf(DEBUG_INFO,"*****Image Header Table Details******** \n\r");
	XFsbl_Printf(DEBUG_INFO,"Boot Gen Ver: 0x%0lx \n\r",
			ImageHeaderTable->Version);
	XFsbl_Printf(DEBUG_INFO,"No of Partitions: 0x%0lx \n\r",
			ImageHeaderTable->NoOfPartitions);
	XFsbl_Printf(DEBUG_INFO,"Partition Header Address: 0x%0lx \n\r",
			ImageHeaderTable->PartitionHeaderAddress);
	XFsbl_Printf(DEBUG_INFO,"Partition Present Device: 0x%0lx \n\r",
			ImageHeaderTable->PartitionPresentDevice);

END:
	return Status;
}

/****************************************************************************/
/**
 * This function reads the image header from flash device. Image header
 * reads the image header table and partition headers
 *
 * @param ImageHeader pointer to the image header at which image header to
 * be stored
 *
 * @param DeviceOps pointer deviceops structure which contains the
 * function pointer to flash read function
 *
 * @param FlashImageOffsetAddress base offset to the image in the flash
 *
 * @return
 * 	- XFSBL_SUCCESS on successful reading image header
 * 	- errors as mentioned in xfsbl_error.h
 *
 * @note
 *****************************************************************************/
u32 XFsbl_ReadImageHeader(XFsblPs_ImageHeader * ImageHeader,
				XFsblPs_DeviceOps * DeviceOps,
				u32 FlashImageOffsetAddress)
{
	u32 Status = XFSBL_SUCCESS;
	u32 ImageHeaderTableAddressOffset=0U;
	u32 PartitionHeaderAddress=0U;
	u32 PartitionIndex=0U;
	XFsblPs_PartitionHeader *CurrPartitionHdr;
	u32 EntryCount = 0;
	u32 DestnCPU;

	/**
	 * Read the Image Header Table offset from
	 * Boot Header
	 */
	Status = DeviceOps->DeviceCopy(FlashImageOffsetAddress
	            + XIH_BH_IH_TABLE_OFFSET,
		   (PTRSIZE ) &ImageHeaderTableAddressOffset, XIH_FIELD_LEN);
	if (XFSBL_SUCCESS != Status)
	{
		XFsbl_Printf(DEBUG_GENERAL,"Device Copy Failed \n\r");
		goto END;
	}
	XFsbl_Printf(DEBUG_INFO,"Image Header Table Offset 0x%0lx \n\r",
			ImageHeaderTableAddressOffset);

	/**
	 * Read the Image header table of 64 bytes
	 * and update the image header table structure
	 */
	Status = DeviceOps->DeviceCopy(FlashImageOffsetAddress +
				    ImageHeaderTableAddressOffset,
				    (PTRSIZE ) &(ImageHeader->ImageHeaderTable),
				    XIH_IHT_LEN);
	if (XFSBL_SUCCESS != Status)
	{
		XFsbl_Printf(DEBUG_GENERAL,"Device Copy Failed \n\r");
		goto END;
	}


	/**
	 * Check the validity of Image Header Table
	 */
	Status = XFsbl_ValidateImageHeaderTable(
	          &(ImageHeader->ImageHeaderTable));
	if (XFSBL_SUCCESS != Status)
	{
		XFsbl_Printf(DEBUG_GENERAL,"Image Header Table "
				"Validation failed \n\r");
		goto END;
	}

	/**
	 * Update the first partition address
	 */
	PartitionHeaderAddress =
		(ImageHeader->ImageHeaderTable.PartitionHeaderAddress)
			* XIH_PARTITION_WORD_LENGTH;


	/**
	 * Read the partitions based on the partition offset
	 * and update the partition header structure
	 */
	for (PartitionIndex=0U;
		PartitionIndex<ImageHeader->ImageHeaderTable.NoOfPartitions;
		PartitionIndex++)
	{

		/**
		 * Read the Image header table of 64 bytes
		 * and update the image header table structure
		 */
		Status = DeviceOps->DeviceCopy(
	                FlashImageOffsetAddress + PartitionHeaderAddress,
		(PTRSIZE ) &(ImageHeader->PartitionHeader[PartitionIndex]),
			XIH_PH_LEN);
		if (XFSBL_SUCCESS != Status)
		{
			XFsbl_Printf(DEBUG_GENERAL,"Device Copy Failed \n\r");
			goto END;
		}

#if 0
		/**
		 * Check the validity of Image Header Table
		 */
		Status = XFsbl_ValidatePartitionHeader(
				&ImageHeader->PartitionHeader[PartitionIndex]);
		if (XFSBL_SUCCESS != Status)
		{
			XFsbl_Printf(DEBUG_GENERAL,"Partition %x Header "
				"Validation failed \n\r", PartitionIndex);
			goto END;
		}
#endif

		/**
		 * Assumption: Next partition corresponds to ATF
		 * The first partition of an application will have a non zero
		 * execution address. All the remaining partitions of that
		 * application will have 0 as execution address. Hence look for
		 * the non zero execution address for partition which is not
		 * the first one and ensure the CPU is A53
		 */

		CurrPartitionHdr = &ImageHeader->PartitionHeader[PartitionIndex];

		DestnCPU = XFsbl_GetDestinationCpu(CurrPartitionHdr);

		if ((PartitionIndex > 1) && (EntryCount < XFSBL_MAX_ENTRIES_FOR_ATF) &&
				(CurrPartitionHdr->DestinationExecutionAddress != 0U) &&
				(((DestnCPU >= XIH_PH_ATTRB_DEST_CPU_A53_0) &&
						(DestnCPU <= XIH_PH_ATTRB_DEST_CPU_A53_3))))
		{
			/**
			 *  Populate handoff parameters to ATF
			 *  These correspond to the partition of application
			 *  which ATF will be loading
			 */
			XFsbl_SetATFHandoffParameters(CurrPartitionHdr, EntryCount);
			EntryCount++;
		}

		/**
		 * Update the next partition present address
		 */
		PartitionHeaderAddress =
		 (ImageHeader->PartitionHeader[PartitionIndex].NextPartitionOffset)
			  * XIH_PARTITION_WORD_LENGTH;
	}

	/**
	 * After setting handoff parameters of all partitions to ATF,
	 * Store lower address of the structure at Persistent register 4
	 * and higher address at Persistent register 5
	 */
	XFsbl_Out32(LPD_SLCR_PERSISTENT4,(u32)(((PTRSIZE)(&ATFHandoffParams)) & 0xFFFFFFFF));
	XFsbl_Out32(LPD_SLCR_PERSISTENT5, (u32)(((PTRSIZE)(&ATFHandoffParams)) >> 32));

END:
	return Status;
}


static u32 XFsbl_CheckValidMemoryAddress(u64 Address, u32 CpuId, u32 DevId)
{
	u32 Status = XFSBL_SUCCESS;

	/* Check if Address is in the range of PMU RAM for PMU FW */
	if (DevId == XIH_PH_ATTRB_DEST_DEVICE_PMU)
	{
		if ((Address >= XFSBL_PMU_RAM_START_ADDRESS) &&
			(Address < XFSBL_PMU_RAM_END_ADDRESS) )
		{
			goto END;
		}
	}

	/**
	 * Check if Address is in the range of TCM for R5
	 */
	if ((CpuId == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
		(CpuId == XIH_PH_ATTRB_DEST_CPU_R5_1) ||
		(CpuId == XIH_PH_ATTRB_DEST_CPU_R5_L) )
	{
		if ((Address >= XFSBL_R5_TCM_START_ADDRESS) &&
			(Address < XFSBL_R5_TCM_END_ADDRESS) )
		{
			goto END;
		}
	}

#ifdef XFSBL_PS_DDR
	/**
	 * Check if Address is in the range of PS DDR
	 */
	if ((Address >= XFSBL_PS_DDR_START_ADDRESS) &&
	     (Address < XFSBL_PS_DDR_END_ADDRESS) )
	{
		goto END;
	}
#endif

#ifdef XFSBL_PL_DDR
	/**
	 * Check if Address is in the range of PS DDR
	 */
	if ((Address >= XFSBL_PL_DDR_START_ADDRESS) &&
	     (Address < XFSBL_PL_DDR_END_ADDRESS) )
	{
		goto END;
	}
#endif

#ifdef XFSBL_OCM
	 /**
         * Check if Address is in the range of last bank of OCM
         */
        if ((Address >= XFSBL_OCM_START_ADDRESS) &&
             (Address < XFSBL_OCM_END_ADDRESS) )
        {
                goto END;
        }
#endif


	/**
	 * Not a valid address
	 */
	Status = XFSBL_ERROR_ADDRESS;
	XFsbl_Printf(DEBUG_GENERAL,
		"XFSBL_ERROR_ADDRESS\n\r");
END:
	return Status;
}


/****************************************************************************/
/**
* This function validates the partition header.
*
* @param PartitionHeader is pointer to the XFsblPs_PartitionHeader structure
*
* @return
* 	- XFSBL_SUCCESS on successful partition header validation
* 	- errors as mentioned in xfsbl_error.h
*
* @note
*
*****************************************************************************/
u32 XFsbl_ValidatePartitionHeader(
	XFsblPs_PartitionHeader * PartitionHeader, u32 RunningCpu)
{
	u32 Status = XFSBL_SUCCESS;
	u32 DestinationCpu=0U;
	u32 DestinationDevice=0;
	s32 IsEncrypted=FALSE, IsAuthenticated=FALSE;


	/**
	 * Update the variables
	 */
	if (XFsbl_IsEncrypted(PartitionHeader) ==
			XIH_PH_ATTRB_ENCRYPTION )
	{
		IsEncrypted = TRUE;
	}

	if (XFsbl_IsRsaSignaturePresent(PartitionHeader) ==
			XIH_PH_ATTRB_RSA_SIGNATURE )
	{
		IsAuthenticated = TRUE;
	}


	DestinationCpu = XFsbl_GetDestinationCpu(PartitionHeader);
	DestinationDevice = XFsbl_GetDestinationDevice(PartitionHeader);
	/**
	 * Partition fields Validation
	 */


	/**
	 * check for XIP image - partition lengths should be zero
	 * execution address should be in QSPI
	 */
	if (PartitionHeader->UnEncryptedDataWordLength == 0U)
	{
		if ((IsAuthenticated ==   TRUE ) ||
			(IsEncrypted ==	TRUE ))
		{
			Status = XFSBL_ERROR_XIP_AUTH_ENC_PRESENT;
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_XIP_AUTH_ENC_PRESENT\n\r");
			goto END;
		}

		if ((PartitionHeader->DestinationExecutionAddress
				< XFSBL_QSPI_LINEAR_BASE_ADDRESS_START) ||
				(PartitionHeader->DestinationExecutionAddress
					> XFSBL_QSPI_LINEAR_BASE_ADDRESS_END))
		{
			Status = XFSBL_ERROR_XIP_EXEC_ADDRESS;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_XIP_EXEC_ADDRESS\n\r");
			goto END;
		}
	}


	/**
	 * check for authentication and encryption length
	 */
	if ((IsAuthenticated == FALSE ) &&
			(IsEncrypted ==	FALSE ))
	{
		/**
		 * all lengths should be equal
		 */
		if ((PartitionHeader->UnEncryptedDataWordLength !=
				PartitionHeader->EncryptedDataWordLength) ||
				(PartitionHeader->EncryptedDataWordLength !=
					PartitionHeader->TotalDataWordLength))
		{
			Status = XFSBL_ERROR_PARTITION_LENGTH;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_PARTITION_LENGTH\n\r");
			goto END;
		}
	} else if ((IsAuthenticated == TRUE ) &&
			(IsEncrypted ==	FALSE ))
	{
		/**
		 * TotalDataWordLength should be more
		 */
		if ((PartitionHeader->UnEncryptedDataWordLength !=
				PartitionHeader->EncryptedDataWordLength) ||
				(PartitionHeader->EncryptedDataWordLength >=
					PartitionHeader->TotalDataWordLength))
		{
			Status = XFSBL_ERROR_PARTITION_LENGTH;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_PARTITION_LENGTH\n\r");
			goto END;
		}
	} else if ((IsAuthenticated == FALSE ) &&
			(IsEncrypted ==	TRUE ))
	{
		/**
		 * EncryptedDataWordLength should be more
		 */
		if ((PartitionHeader->UnEncryptedDataWordLength >=
				PartitionHeader->EncryptedDataWordLength) ||
				(PartitionHeader->EncryptedDataWordLength !=
					PartitionHeader->TotalDataWordLength))
		{
			Status = XFSBL_ERROR_PARTITION_LENGTH;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_PARTITION_LENGTH\n\r");
			goto END;
		}
	} else /* Authenticated and Encrypted */
	{
		/**
		 * TotalDataWordLength should be more
		 */
		if ((PartitionHeader->UnEncryptedDataWordLength >=
				PartitionHeader->EncryptedDataWordLength) ||
				(PartitionHeader->EncryptedDataWordLength >=
					PartitionHeader->TotalDataWordLength))
		{
			Status = XFSBL_ERROR_PARTITION_LENGTH;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_PARTITION_LENGTH\n\r");
			goto END;
		}
	}


	/**
	 * handled on partition copy block
	 * Check for Destination Load Address,
	 * executable address
	 * Address not in TCM, OCM, DDR, PL
	 *  No DDR and address in DDR
	 */
	Status = XFsbl_CheckValidMemoryAddress(
                        PartitionHeader->DestinationLoadAddress,
                        DestinationCpu, DestinationDevice);
	if (Status != XFSBL_SUCCESS)
	{
		goto END;
	}

	/**
	 * R5 can't access the DDR 0 address as TCM is present there
	 */
	if ( ((RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
	     (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_L))
	     &&
	    ((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_0) ||
	     (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_1) ||
	     (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_2) ||
	     (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_A53_3) ))
	{
		/**
		 * DDR address for A53-x should be greater than TCM
		 */
		if (PartitionHeader->DestinationLoadAddress < 0x80000U)
		{
			Status = XFSBL_ERROR_ADDRESS;
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_ADDRESS\n\r");
			goto END;
		}
	}


	/**
	 * Checks for invalid checksum type
	 */
	if ((XFsbl_GetChecksumType(PartitionHeader) !=
	                      XIH_PH_ATTRB_NOCHECKSUM)
	          && (XFsbl_GetChecksumType(PartitionHeader) !=
	                      XIH_PH_ATTRB_CHECKSUM_MD5))
	{
		Status = XFSBL_ERROR_INVALID_CHECKSUM_TYPE;
		XFsbl_Printf(DEBUG_GENERAL,
		    "XFSBL_ERROR_INVALID_CHECKSUM_TYPE\n\r");
		goto END;
	}

	/**
	 * check for invalid cpu
	 */
	if (DestinationCpu > XIH_PH_ATTRB_DEST_CPU_R5_L)
	{
		Status = XFSBL_ERROR_INVALID_CPU_TYPE;
		XFsbl_Printf(DEBUG_GENERAL,
		    "XFSBL_ERROR_INVALID_CPU_TYPE\n\r");
		goto END;
	}


	/**
	 * check if
	 *  1. if FSBL on R5-0 and Destination CPU is R5-L
	 *  2. if FSBL on R5-L and Destination CPU is R5-0/R5-1
	 */
	if (
	    (((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_0) ||
	    (DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_1)) &&
	    (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_L) ) ||

	    ((DestinationCpu == XIH_PH_ATTRB_DEST_CPU_R5_L) &&
	    (RunningCpu == XIH_PH_ATTRB_DEST_CPU_R5_0))
	   )
	{
		Status = XFSBL_ERROR_LS_CPU_TYPE;
		XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_LS_CPU_TYPE\n\r");
		goto END;

	}

	/**
	 * check for invalid destination device
	 */
	if (XFsbl_GetDestinationDevice(PartitionHeader) >
				XIH_PH_ATTRB_DEST_DEVICE_PMU)
	{
		Status = XFSBL_ERROR_INVALID_DEST_DEVICE;
		XFsbl_Printf(DEBUG_GENERAL,
		    "XFSBL_ERROR_INVALID_DEST_DEVICE\n\r");
		goto END;
	}

	/**
	 * Print Partition Header Details
	 */
	XFsbl_Printf(DEBUG_INFO,"UnEncrypted data Length: 0x%0lx \n\r",
				PartitionHeader->UnEncryptedDataWordLength);
	XFsbl_Printf(DEBUG_INFO,"Data word offset: 0x%0lx \n\r",
				PartitionHeader->EncryptedDataWordLength);
	XFsbl_Printf(DEBUG_INFO,"Total Data word length: 0x%0lx \n\r",
				PartitionHeader->TotalDataWordLength);
	XFsbl_Printf(DEBUG_INFO,"Destination Load Address: 0x%0lx \n\r",
			(PTRSIZE)PartitionHeader->DestinationLoadAddress);
	XFsbl_Printf(DEBUG_INFO,"Execution Address: 0x%0lx \n\r",
			(PTRSIZE)PartitionHeader->DestinationExecutionAddress);
	XFsbl_Printf(DEBUG_INFO,"Data word offset: 0x%0lx \n\r",
				PartitionHeader->DataWordOffset);
	XFsbl_Printf(DEBUG_INFO,"Partition Attributes: 0x%0lx \n\r",
				PartitionHeader->PartitionAttributes);

END:
	return Status;
}

/****************************************************************************/
/**
* This function sets the handoff parameters to the ARM Trusted Firmware (ATF)
* Some of the inputs for this are taken from FSBL partition header
* A pointer to the structure containing these parameters is stored in
* persistent register 5, which ATF reads
*
* @param PartitionHeader is pointer to the XFsblPs_PartitionHeader structure
*
* @return None
*
* @note
*
*****************************************************************************/
static void XFsbl_SetATFHandoffParameters(
		XFsblPs_PartitionHeader *PartitionHeader, u32 EntryCount)
{
	u32 PartitionAttributes;
	u64 PartitionFlags;

	PartitionAttributes = PartitionHeader->PartitionAttributes;

	PartitionFlags =
		(((PartitionAttributes & XIH_PH_ATTRB_A53_EXEC_ST_MASK)
				>> XIH_ATTRB_A53_EXEC_ST_SHIFT_DIFF) |
		((PartitionAttributes & XIH_PH_ATTRB_ENDIAN_MASK)
				>> XIH_ATTRB_ENDIAN_SHIFT_DIFF) |
		((PartitionAttributes & XIH_PH_ATTRB_TR_SECURE_MASK)
				<< XIH_ATTRB_TR_SECURE_SHIFT_DIFF) |
		((PartitionAttributes & XIH_PH_ATTRB_TARGET_EL_MASK)
				<< XIH_ATTRB_TARGET_EL_SHIFT_DIFF) |
		((PartitionAttributes & XIH_PH_ATTRB_DEST_CPU_A53_MASK)
				>> XIH_ATTRB_DEST_CPU_A53_SHIFT_DIFF));

	/* Insert magic string */
	if (EntryCount == 0)
	{
		ATFHandoffParams.MagicValue[0] = 'X';
		ATFHandoffParams.MagicValue[1] = 'L';
		ATFHandoffParams.MagicValue[2] = 'N';
		ATFHandoffParams.MagicValue[3] = 'X';
	}

	ATFHandoffParams.NumEntries = EntryCount + 1;

	ATFHandoffParams.Entry[EntryCount].EntryPoint =
			PartitionHeader->DestinationExecutionAddress;
	ATFHandoffParams.Entry[EntryCount].PartitionFlags = PartitionFlags;

}
