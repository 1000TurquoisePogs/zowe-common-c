

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE 
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include "metalio.h"
#else
#include <stdio.h>
#include <ctest.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  
#endif

#include "copyright.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "bigint.h"
#include "icsf.h"

#ifndef _LP64
#pragma linkage(CSNBOWH,OS)
#else
#pragma linkage(CSNEOWH,OS)
#endif

/*
  Encipher/Decipher
   
   Symmetric Key Decipher (CSNBSYD and CSNBSYD1)
   Symmetric Key Encipher (CSNBSYE and CSNBSYE1)

   Support 
     Ciphers:  AES, DES, 3DES
     Modes:    CBC, ECB, CFB
     Paddding  PKCS-PAD


CALL CSNBSYE( return_code, 
              reason_code, 
	      exit_data_length,      -- ignored
	      exit_data,             -- ignored
	      rule_array_count,     -- usually 5
	      rule_array,           "AES|DES  " followed by 
	                            
                                    "CBC|ECB|CFB      "

				    followed by

				    "PKCS-PAD|X9.23   " or nothing
				    
				    SSL has its own funky padding scheme for block ciphers
				        that can be seen in the java implementation 

				    followed by 

				    "KEY-CLR " or "KEYIDENT" if key is stored in token/key store thing
				    
                                    end with "INITIAL " or "CONTINUE"


	      key_length,           -- for AES is 16,24,32 chainData 32
				    -- for DES is 8,16,24 chainData 16
	      key_identifier,       -- key data or token
	      key_parms_length,     -- assuming 0 for our favorites
	      key_parms,            -- assume NULL
	      block_size,           -- DES->8 AES->16
	      initialization_vector_length,  -- same as block length
	      initialization_vector,         -- random from key exchange
	      chain_data_length,             -- ocv followed by icv
	      chain_data, 
	      clear_text_length,             -- who should do the padding???
	      clear_text, 
	      cipher_text_length, 
	      cipher_text, 
	      optional_data_length,  -- assume 0
	      optional_data)         -- assume NULLA


   public KEYs

   RSA

   Diffie-Helmann
  */

static int digestTrace = FALSE;

int icsfDigestInit(ICSFDigest *digest, int type){
  memset((char*)digest,0,sizeof(ICSFDigest));
  digest->type = type;
  switch (type){
  case ICSF_DIGEST_MD5:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "MD5     FIRST   ";
    digest->hashLength = 16;
    break;
  case ICSF_DIGEST_SHA1:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "SHA-1   FIRST   ";
    digest->hashLength = 20;
    break;
  default:
    printf("unimplemented\n");
    return 8;
  }
  int returnCode;
  int reasonCode;
  
  int exitDataLength = 0;
  char *exitData = NULL;
  int contextLength = ICSF_HASH_CONTEXT_LENGTH;
  int zero = 0;
  char *null = NULL;

#ifdef _LP64
  CSNEOWH
#else
  CSNBOWH
#endif
   (&returnCode, &reasonCode,
	  &exitDataLength, exitData,
	  &digest->ruleArrayCount,digest->ruleArray,
	  &zero,null,
	  &contextLength,digest->context,	  
	  &digest->hashLength,digest->hash);
  if (digestTrace){
    printf("CSNBOWH (FIRST) retcode=%d reason=%d leftoverFill=%d\n",
	   returnCode,reasonCode,digest->leftoverFill);
  }

  return returnCode;  
}

int icsfDigestUpdate(ICSFDigest *digest, char *data, int len){
  switch (digest->type){
  case ICSF_DIGEST_MD5:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "MD5     MIDDLE  ";
    break;
  case ICSF_DIGEST_SHA1:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "SHA-1   MIDDLE  ";
    break;
  default:
    printf("unimplemented ICSF Digest type\n");
    return 8;
  }
  int returnCode;
  int reasonCode;
  char *null = NULL;
  int exitDataLength = 0;
  char *exitData = NULL;
  int contextLength = ICSF_HASH_CONTEXT_LENGTH;

  int newFill = digest->leftoverFill + len;
  if (digestTrace){
    printf("update, leftoverFill=%d len=%d newFill = %d\n",digest->leftoverFill,len,newFill);
  }
  if (newFill < ICSF_HASH_BLOCK_LENGTH){
    memcpy(digest->leftovers+digest->leftoverFill,data,len);
    digest->leftoverFill += len;
    if (digestTrace){
      printf("hash update case 1, leftoverFill = %d\n",digest->leftoverFill);
      dumpbuffer(digest->leftovers,digest->leftoverFill);
    }
    return 0;
  } else{
    int blocksToHash = newFill / ICSF_HASH_BLOCK_LENGTH;
    int newLeftoverFill = newFill % ICSF_HASH_BLOCK_LENGTH;
    int i;
    int dataPos = 0;
    int hashBlockLength = ICSF_HASH_BLOCK_LENGTH;
    char temp[ICSF_HASH_BLOCK_LENGTH];

    memset(temp,0,64);
    if (digestTrace){
      printf("update, blocksToHash = %d\n",blocksToHash);
    }
    for (i=0; i<blocksToHash; i++){
      if (i==0){
	memcpy(temp,digest->leftovers,digest->leftoverFill);
	dataPos = (ICSF_HASH_BLOCK_LENGTH - digest->leftoverFill);
	memcpy(temp+digest->leftoverFill,data,dataPos);
      } else{
	memcpy(temp,data+dataPos,ICSF_HASH_BLOCK_LENGTH);
	dataPos += ICSF_HASH_BLOCK_LENGTH;
      }

#ifdef _LP64
  CSNEOWH
#else
  CSNBOWH
#endif
       (&returnCode, &reasonCode,
	      &exitDataLength, exitData,
	      &digest->ruleArrayCount,digest->ruleArray,
	      &hashBlockLength,temp,
	      &contextLength,digest->context,	  
	      &digest->hashLength,digest->hash);
      if (digestTrace){
	printf("CSNBOWH retcode=%d reason=%d\n",returnCode,reasonCode);
      }
    }
    memcpy(digest->leftovers,data+(len-newLeftoverFill),newLeftoverFill);
    digest->leftoverFill = newLeftoverFill;
    if (digestTrace){
      printf("hash update case 2, leftoverFill = %d\n",digest->leftoverFill);
      dumpbuffer(digest->leftovers,digest->leftoverFill);
    }
    return returnCode;  
  }
}

int icsfDigestFinish(ICSFDigest *digest, char *hash){
  switch (digest->type){
  case ICSF_DIGEST_MD5:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "MD5     LAST    ";
    break;
  case ICSF_DIGEST_SHA1:
    digest->ruleArrayCount = 2;
    digest->ruleArray = "SHA-1   LAST    ";
    break;
  default:
    printf("unimplemented\n");
    return 8;
  }
  int returnCode;
  int reasonCode;

  int exitDataLength = 0;
  char *exitData = NULL;
  int contextLength = ICSF_HASH_CONTEXT_LENGTH;
  char *s = digest->leftovers;
  int len = digest->leftoverFill;

#ifdef _LP64
  CSNEOWH
#else
  CSNBOWH
#endif
   (&returnCode, &reasonCode,
	  &exitDataLength, exitData,
	  &digest->ruleArrayCount,digest->ruleArray,
	  &len,s,
	  &contextLength,digest->context,	  
	  &digest->hashLength,digest->hash);
  if (digestTrace){
    printf("CSNBOWH (LAST) retcode=%d reason=%d\n",returnCode,reasonCode);
  }
  memcpy(hash,digest->hash,digest->hashLength);
  return returnCode;  
}

int icsfDigestFully(char *digestType, char *s, int len){
  int returnCode;
  int reasonCode;

  int exitDataLength = 0;
  char *exitData = NULL;
  int ruleArrayCount = 2;

  char *ruleArray = "MD5     ONLY    ";
  int chainingVectorLength = 128; /* stipulated by documentation */
  char chainingVector[128];
  int md5HashLength = 16;
  char hash[20];
  if (!strcmp(digestType,"SHA")){
    char *ruleArray = "SHA-1   ONLY    ";
  }
  int zero = 0;
  memset(chainingVector,0,128);
  
#ifdef _LP64
  CSNEOWH
#else
  CSNBOWH
#endif
   (&returnCode,
	  &reasonCode,

	  &exitDataLength,
	  exitData,

	  &ruleArrayCount,
	  ruleArray,

	  &len,
	  s,

	  &chainingVectorLength,
	  chainingVector,

	  &md5HashLength,
	  hash);

  printf("called ICSF MD5 ret=0x%x reason=0x%x\n",returnCode,reasonCode);
  dumpbuffer(hash,16);
	  
  return returnCode;
  
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

