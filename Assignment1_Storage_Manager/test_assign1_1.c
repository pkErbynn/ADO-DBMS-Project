#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage_mgr.h"
#include "dberror.h"
#include "test_helper.h"

// test name
char *testing_name;

/* test output files */
#define TESTPF "test_pagefile.bin"

/* prototypes for test functions */
static void testCreateOpenClose(void);
static void testSinglePageContent(void);

static void testofgetblockPos(void);
static void testBlockMapReading(void);
static void testofWriteCurrentBlock(void);
static void testofAppendEmptyBlock(void);
static void testofEnsureCapacity(void);


/* main function running all tests */
int
main (void)
{
  testing_name = "";
  
  initStorageManager();

  testCreateOpenClose();
  testSinglePageContent();

  printf("\033[1;3;34m Additional Tests \033[0m\n");
  testofgetblockPos();
  testBlockMapReading();
  testofWriteCurrentBlock();
  testofAppendEmptyBlock();
  testofEnsureCapacity();
  printf("\033[32mALL TESTS ARE PASSED SUCCESSFULLY !\n\033[0m");
  return 0;
}


/* check a return code. If it is not RC_OK then output a message, error description, and exit */
/* Try to create, open, and close a page file */
void
testCreateOpenClose(void)
{
  SM_FileHandle fl_hdl;

  testing_name = "test create open and close methods";

  TEST_CHECK(createPageFile (TESTPF));
  
  TEST_CHECK(openPageFile (TESTPF, &fl_hdl));
  ASSERT_TRUE(strcmp(fl_hdl.fileName, TESTPF) == 0, "filename correct");
  ASSERT_TRUE((fl_hdl.totalNumPages == 1), "expect 1 page in new file");
  ASSERT_TRUE((fl_hdl.curPagePos == 0), "freshly opened file's page position should be 0");

  TEST_CHECK(closePageFile (&fl_hdl));
  TEST_CHECK(destroyPageFile (TESTPF));

  // after destruction trying to open the file should cause an error
  ASSERT_TRUE((openPageFile(TESTPF, &fl_hdl) != RC_OK), "opening non-existing file should return an error.");

  TEST_DONE();
}

/* Try to create, open, and close a page file */
void
testSinglePageContent(void)
{
  SM_FileHandle fl_hdl;
  SM_PageHandle pg_hdl;
  int i;

  testing_name = "test single page content";

  pg_hdl = (SM_PageHandle) malloc(PAGE_SIZE);

  // create a new page file
  TEST_CHECK(createPageFile (TESTPF));
  TEST_CHECK(openPageFile (TESTPF, &fl_hdl));
  printf("created and opened file\n");
  
  // read first page into handle
  TEST_CHECK(readFirstBlock (&fl_hdl, pg_hdl));
  // the page should be empty (zero bytes)
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((pg_hdl[i] == 0), "expected zero byte in first page of freshly initialized page");
  printf("first block was empty\n");
    
  // change pg_hdl to be a string and write that one to disk
  for (i=0; i < PAGE_SIZE; i++)
    pg_hdl[i] = (i % 10) + '0';
  TEST_CHECK(writeBlock (0, &fl_hdl, pg_hdl));
  printf("writing first block\n");

  // read back the page containing the string and check that it is correct
  TEST_CHECK(readFirstBlock (&fl_hdl, pg_hdl));
  for (i=0; i < PAGE_SIZE; i++)
    ASSERT_TRUE((pg_hdl[i] == (i % 10) + '0'), "character in page read from disk is the one we expected.");
  printf("reading first block\n");
  TEST_CHECK(closePageFile (&fl_hdl));

  // destroy new page file
  TEST_CHECK(destroyPageFile (TESTPF));  
  
  TEST_DONE();
}

void testofgetblockPos(void) {
    SM_FileHandle fl_hdl;

    TEST_CHECK(createPageFile(TESTPF));
    TEST_CHECK(openPageFile(TESTPF, &fl_hdl));
    
    // After opening, the current block position should be 0
    ASSERT_TRUE(getBlockPos(&fl_hdl) == 0, "After opening a file, the current block position is not zero.");

    TEST_CHECK(closePageFile(&fl_hdl));
    TEST_CHECK(destroyPageFile(TESTPF));

    TEST_DONE();
}


/* Test reading previous, current, next, and last block */
void testBlockMapReading(void) {
    SM_FileHandle fl_hdl;
    SM_PageHandle pg_hdl = (SM_PageHandle)malloc(PAGE_SIZE);

    // Setup - Create a file and write some data to multiple pages
    TEST_CHECK(createPageFile(TESTPF));
    TEST_CHECK(openPageFile(TESTPF, &fl_hdl));
    for(int i = 0; i < 4; i++) { // ensure 4 pages exist
        TEST_CHECK(appendEmptyBlock(&fl_hdl));

    }

    TEST_CHECK(writeBlock(4, &fl_hdl, pg_hdl)); // write to the last block to increase total pages

    // Test reading the last block
    TEST_CHECK(readLastBlock(&fl_hdl, pg_hdl));
    ASSERT_TRUE(getBlockPos(&fl_hdl) == 4, "It did not move to the last block");

    // Test reading the next block from position 4 should fail as it does not exist
    ASSERT_TRUE(readNextBlock(&fl_hdl, pg_hdl) != RC_OK, "Non-existing next block");

    // Test reading the previous block from position 4, should succeed
    TEST_CHECK(readPreviousBlock(&fl_hdl, pg_hdl));
    ASSERT_TRUE(getBlockPos(&fl_hdl) == 3, "It did not move to the previous block");

    // Test reading the current block
    TEST_CHECK(readCurrentBlock(&fl_hdl, pg_hdl));
    ASSERT_TRUE(getBlockPos(&fl_hdl) == 3, "Current block position is incorrect");

    // Move to first block and test reading the next block
    TEST_CHECK(readFirstBlock(&fl_hdl, pg_hdl));
    TEST_CHECK(readNextBlock(&fl_hdl, pg_hdl));
    ASSERT_TRUE(getBlockPos(&fl_hdl) == 1, "It did not move to the next block after the first block");

    TEST_CHECK(closePageFile(&fl_hdl));
    TEST_CHECK(destroyPageFile(TESTPF));

    TEST_DONE();
}

/* Test writing to the current block */
void testofWriteCurrentBlock(void) {
    SM_FileHandle fl_hdl;
    SM_PageHandle pg_hdl = (SM_PageHandle)malloc(PAGE_SIZE);

    // Initialize page handle with some data
    for (int i = 0; i < PAGE_SIZE; i++) {
      pg_hdl[i] = 'A';
    }

    TEST_CHECK(createPageFile(TESTPF));
    TEST_CHECK(openPageFile(TESTPF, &fl_hdl));

    // Write to the current block (which is the first block)
    TEST_CHECK(writeCurrentBlock(&fl_hdl, pg_hdl));
    // Read back to verify
    TEST_CHECK(readFirstBlock(&fl_hdl, pg_hdl));
    for (int i = 0; i < PAGE_SIZE; i++) {
        ASSERT_TRUE(pg_hdl[i] == 'A', "Data written and read from the current block are not matching");

    }

    TEST_CHECK(closePageFile(&fl_hdl));
    TEST_CHECK(destroyPageFile(TESTPF));
    free(pg_hdl);

    TEST_DONE();
}

/* Test appending an empty block */
void testofAppendEmptyBlock(void) {
    SM_FileHandle fl_hdl;
    int ini_ct_of_pgs;

    TEST_CHECK(createPageFile(TESTPF));
    TEST_CHECK(openPageFile(TESTPF, &fl_hdl));

    ini_ct_of_pgs = fl_hdl.totalNumPages;
    
    // Append an empty block and verify the total number of pages increased
    TEST_CHECK(appendEmptyBlock(&fl_hdl));
    ASSERT_TRUE(fl_hdl.totalNumPages == ini_ct_of_pgs + 1, "Total number of pages did not increase after appending an empty block");

    TEST_CHECK(closePageFile(&fl_hdl));
    TEST_CHECK(destroyPageFile(TESTPF));

    TEST_DONE();
}

/* Test ensuring capacity */
void testofEnsureCapacity(void) {
    SM_FileHandle fl_hdl;
    int pgs_reqd = 5;

    TEST_CHECK(createPageFile(TESTPF));
    TEST_CHECK(openPageFile(TESTPF, &fl_hdl));

    // Ensure the file has at least 5 pages
    TEST_CHECK(ensureCapacity(pgs_reqd, &fl_hdl));
    
    // Verify the file now has at least 5 pages
    ASSERT_TRUE(fl_hdl.totalNumPages >= pgs_reqd, "Even after verifying capacity, the file does not have the necessary number of pages.");

    // Try reading the last page to ensure it exists
    SM_PageHandle pg_hdl = (SM_PageHandle)malloc(PAGE_SIZE);
    TEST_CHECK(readBlock(pgs_reqd - 1, &fl_hdl, pg_hdl));
    free(pg_hdl);

    TEST_CHECK(closePageFile(&fl_hdl));
    TEST_CHECK(destroyPageFile(TESTPF));

    TEST_DONE();
}