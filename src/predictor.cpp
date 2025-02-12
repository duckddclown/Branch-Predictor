//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Ziyuan Lin";
const char *studentID = "A59025230";
const char *email = "zil102@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 17; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

//Tournament bits.
int ghistoryBits_tournament = 16;
int lhistoryBits = 14;
int pcIndexBits = 10;
int choicerBits = 15;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

// Alpha
uint8_t* bht_tournament_global;
uint8_t* bht_tournament_local;
uint64_t* lht_tournament;
uint8_t* choicer;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

//Tournament functions.

void init_tournament()
{
  int global_bht_entries = 1 << ghistoryBits_tournament;
  int local_bht_entries = 1 << lhistoryBits;
  int choicer_entries = 1 << choicerBits;
  int pc_entries = 1 << pcIndexBits;
  bht_tournament_global = (uint8_t *)malloc(global_bht_entries * sizeof(uint8_t));
  bht_tournament_local = (uint8_t *)malloc(local_bht_entries * sizeof(uint8_t));
  choicer = (uint8_t *)malloc(choicer_entries * sizeof(uint8_t));
  lht_tournament = (uint64_t *)malloc(pc_entries * sizeof(uint64_t));
  ghistory = 0;
  int i = 0;
  for (i = 0; i < global_bht_entries; i++)
  {
    bht_tournament_global[i] = WN;
  }
  for (i = 0; i < choicer_entries; i++)
  {
    choicer[i] = WN;
  }
  for (i = 0; i < local_bht_entries; i++)
  {
    bht_tournament_local[i] = WN;
  }
  for (i = 0; i < pc_entries; i++)
  {
    lht_tournament[i] = 0;
  }
}

uint8_t tournament_predict(uint32_t pc)
{
  uint32_t pc_entries = 1 << pcIndexBits;
  uint32_t local_bht_entries = 1 << lhistoryBits;
  uint32_t choicer_entries = 1 << choicerBits;
  uint32_t pc_lower_bits = pc & (pc_entries - 1);
  uint64_t local_history_pattern = lht_tournament[pc_lower_bits];
  local_history_pattern = local_history_pattern & (local_bht_entries-1);
  uint8_t local_branch_history = bht_tournament_local[local_history_pattern];
  uint32_t global_bht_entries = 1 << ghistoryBits_tournament;
  pc_lower_bits = pc & (global_bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (global_bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  uint8_t global_branch_history = bht_tournament_global[index];
  uint32_t choicer_index = index & (choicer_entries-1);
  // If choicer is WT or ST, then we choose to use global one.
  if (choicer[choicer_index] > 1)
  {
    switch (global_branch_history)
    {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament BHT!\n");
      return NOTTAKEN;
    }
  }
  else
  {
    switch (local_branch_history)
    {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament BHT!\n");
      return NOTTAKEN;
    }
  }
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
  uint32_t pc_entries = 1 << pcIndexBits;
  uint32_t pc_lower_bits = pc & (pc_entries - 1);
  uint32_t choicer_entries = 1 << choicerBits;
  uint32_t local_bht_entries = 1 << lhistoryBits;
  uint64_t local_history_pattern = lht_tournament[pc_lower_bits];
  local_history_pattern = local_history_pattern & (local_bht_entries-1);
  uint8_t local_branch_history = bht_tournament_local[local_history_pattern];
  uint32_t global_bht_entries = 1 << ghistoryBits_tournament;
  pc_lower_bits = pc & (global_bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (global_bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  uint8_t global_branch_history = bht_tournament_global[index];
  uint32_t choicer_index = index & (choicer_entries-1);
  switch (local_branch_history)
  {
  case WN:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_tournament_local[local_history_pattern] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in Tournament BHT!\n");
    break;
  }
  switch (global_branch_history)
  {
  case WN:
    bht_tournament_global[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_tournament_global[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_tournament_global[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_tournament_global[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in Tournament BHT!\n");
    break;
  }
  pc_lower_bits = pc & (pc_entries - 1);
  lht_tournament[pc_lower_bits] = ((lht_tournament[pc_lower_bits] << 1) | outcome);
  ghistory = ((ghistory << 1) | outcome);
  if (global_branch_history != local_branch_history)
  {
    if (global_branch_history == outcome)
    {
      switch (choicer[choicer_index])
      {
      case SN:
        choicer[choicer_index] = WN;
        break;
      case WN:
        choicer[choicer_index] = WT;
        break;
      case WT:
        choicer[choicer_index] = ST;
        break;
      case ST:
        choicer[choicer_index] = ST;
        break;
      default:
        printf("Warning: Undefined state of entry in choicer!\n");
        break;
      }
    }
    else
    {
      switch (choicer[choicer_index])
      {
      case SN:
        choicer[choicer_index] = SN;
        break;
      case WN:
        choicer[choicer_index] = SN;
        break;
      case WT:
        choicer[choicer_index] = WN;
        break;
      case ST:
        choicer[choicer_index] = WT;
        break;
      default:
        printf("Warning: Undefined state of entry in choicer!\n");
        break;
      }
    }
  }
}

void cleanup_tournament()
{
  free(bht_tournament_global);
  free(bht_tournament_local);
  free(lht_tournament);
  free(choicer);
}

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    return NOTTAKEN;
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      return;
    default:
      break;
    }
  }
}
