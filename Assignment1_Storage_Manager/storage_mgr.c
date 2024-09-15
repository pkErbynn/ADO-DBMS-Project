#include "storage_mgr.h"
#include "dberror.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void initStorageManager(void) {
    printf("Storage Manager initialized.\n");
}
RC createPageFile(char *fileName) {
    FILE *file = fopen(fileName, "w+");
    if (file == NULL) {
        return RC_FILE_NOT_FOUND; 
    }

    SM_PageHandle emp_pg = calloc(PAGE_SIZE, 1);
    if (emp_pg == NULL) {
        fclose(file);
        return RC_WRITE_FAILED;
    }

    size_t writeSize = fwrite(emp_pg, sizeof(char), PAGE_SIZE, file);
    free(emp_pg);
    fclose(file);

    if (writeSize < PAGE_SIZE) {
        return RC_WRITE_FAILED;
    }
    return RC_OK;
}

RC openPageFile(char *fileName, SM_FileHandle *fileHandle) {
    fileHandle->mgmtInfo = fopen(fileName, "r+");
    if (fileHandle->mgmtInfo == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    fileHandle->fileName = strdup(fileName);
    fileHandle->curPagePos = 0;
    
    fseek(fileHandle->mgmtInfo, 0, SEEK_END);
    long fileSize = ftell(fileHandle->mgmtInfo);
    fileHandle->totalNumPages = fileSize / PAGE_SIZE;
    fseek(fileHandle->mgmtInfo, 0, SEEK_SET);

    return RC_OK;
}


RC closePageFile(SM_FileHandle *fileHandle) {
    if (fileHandle == NULL || fileHandle->mgmtInfo == NULL) {
        printf("Error: File handle or file pointer is NULL\n");
        return RC_FILE_HANDLE_NOT_INIT;
    }

    if (fclose(fileHandle->mgmtInfo) == 0) {
        printf("File closed successfully.\n");
        fileHandle->mgmtInfo = NULL; 
        return RC_OK;
    } else {
        perror("Error closing file");
        return RC_FILE_NOT_FOUND;
    }
}


RC destroyPageFile(char *fileName) {
    if (remove(fileName) != 0) {
        return RC_FILE_NOT_FOUND; 
    }
    return RC_OK;
}

/* Read a block from a page file */
RC readBlock(int pg_no, SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    // Check if the file handle and memory page are valid

    if (fHandle == NULL || mem_area == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Check if the page number is within range
    if (pg_no < 0 || pg_no >= fHandle->totalNumPages) {
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Seek to the page position
    fseek(fHandle->mgmtInfo, pg_no * PAGE_SIZE, SEEK_SET);
    // Read the page into the memory page buffer
    size_t bytesRead = fread(mem_area, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);
    // Check if the read was successful
    if (bytesRead < PAGE_SIZE) {
        return RC_READ_NON_EXISTING_PAGE;
    }

    // Update the file handle's current page position
    fHandle->curPagePos = pg_no;

    return RC_OK;
}

/* Get the current block position */
int getBlockPos(SM_FileHandle *fHandle) {
    return fHandle->curPagePos;
}

/* Read the first block in a page file */
RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    return readBlock(0, fHandle, mem_area);
}

/* Read the previous block in a page file */
RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    int present_pos = getBlockPos(fHandle);
    if (present_pos <= 0) {
        return RC_READ_NON_EXISTING_PAGE;
    }
    return readBlock(present_pos - 1, fHandle, mem_area);
}

/* Read the current block in a page file */
RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    int present_pos = getBlockPos(fHandle);
    return readBlock(present_pos, fHandle, mem_area);
}

/* Read the next block in a page file */
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    int present_pos = getBlockPos(fHandle);
    if (present_pos >= fHandle->totalNumPages - 1) {
        return RC_READ_NON_EXISTING_PAGE;
    }
    return readBlock(present_pos + 1, fHandle, mem_area);
}

/* Read the last block in a page file */
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    return readBlock(fHandle->totalNumPages - 1, fHandle, mem_area);
}

/* Write a block to a page file */
RC writeBlock(int pg_no, SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    // Check if the page number is within range
    if (pg_no < 0 || pg_no >= fHandle->totalNumPages) {
        return RC_WRITE_FAILED;
    }

    // Seek to the page position
    fseek(fHandle->mgmtInfo, pg_no * PAGE_SIZE, SEEK_SET);
    // Write the memory page buffer to the file
    size_t writtenbytes = fwrite(mem_area, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);

    // Check if the write was successful
    if (writtenbytes < PAGE_SIZE) {
        return RC_WRITE_FAILED;
    }

    return RC_OK;
}

/* Write the current block to a page file */
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle mem_area) {
    return writeBlock(fHandle->curPagePos, fHandle, mem_area);
}

/* Append an empty block to a page file */
RC appendEmptyBlock(SM_FileHandle *fHandle) {
    // Create an empty memory page buffer
    SM_PageHandle emp_pg = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (emp_pg == NULL) {
        return RC_WRITE_FAILED;
    }

    // Seek to the end of the file
    fseek(fHandle->mgmtInfo, 0, SEEK_END);
    // Write the empty memory page buffer to the file
    size_t writtenbytes = fwrite(emp_pg, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);
    free(emp_pg);

    // Check if the write was successful
    if (writtenbytes < PAGE_SIZE) {
        return RC_WRITE_FAILED;
    }

    // Increase the total number of pages
    fHandle->totalNumPages++;
    return RC_OK;
}

RC ensureCapacity(int no_of_pgs, SM_FileHandle *fHandle) {
    if (fHandle == NULL || fHandle->mgmtInfo == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }
    int pre_no_of_pgs = fHandle->totalNumPages;
    if (no_of_pgs > pre_no_of_pgs) {
        // Calculate the number of pages to append
        int Additional_pages = no_of_pgs - pre_no_of_pgs;
        // Append empty blocks to the file until it reaches the desired capacity
        for (int i = 0; i < Additional_pages; i++) {
            RC rc = appendEmptyBlock(fHandle);
            if (rc != RC_OK) {
                return rc;
            }
        }
    }
    return RC_OK;
}