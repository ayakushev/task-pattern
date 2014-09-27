// convert_patterns_regexp.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <stdarg.h>
#include "pcre.h"


//
//auxiliary function to print trace to error stream 
//
void PrintLog(const char* fmt_str, ...)
{
	char buf[1024];
	va_list marker;
	va_start (marker, fmt_str );
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt_str, marker );
	va_end  ( marker );
	fprintf( stderr, buf );
}

// patterns for process_pattern_spec function to generate reg ex
//
// regex for matching he the pattern specification
const char *regex_input_pattern = "\\%\\{(.*?)\\}";
const char *regex_start = "^";
const char *regex_end = "$";

//pattern %{#}
const char *pattern_S_simple_token		 = "(?<%d>[\\w|\\s]+)";
const char *pattern_existen_token        = "(\\k<%d>)";
//pattern %{#S#} , has 
const char *pattern_S_multi_token[3] = { 
	"(?<%d>\\b\\w+\\b", //first part
	"\\s\\b\\w+\\b",    //all next are the same
	")" };				//completed part
                               
//pattern %{G}
const char *pattern_G_token = "(?<%d>.+)";


//(^foo blah is a (?<1>\w+)\s{2}(\k<1>)\s{0}$)

#define OVECCOUNT 30    /* should be a multiple of 3 */

//
//change all meta characters in buffer to be treated as a literal characters, except '{','}'
//
//this is not faster method when each char need to compare with meta character array
//in case of big data will be better to prepare 256 char array with indication which symbol is meta char
//
//in: buf - buffer with meta character
//    len - length of string with zero
//    size - function extends buffer till size length max_size
int change_metacharacters(char *buf,int len,int max_size)
{
	//enumerate all symbols
	for( int i=0; i < len && len < max_size && buf && buf[i];i++)
	{
		//search meta character
		if( NULL != strchr("[]\\/^$.|?*+()", buf[i]))
		{
			//insert additional backslash
			memmove(buf + i +1, buf + i, len - i);
			buf[i]='\\';
			i++;
			len++;
		}
	}

	return 0;
}
//
//Processes the pattern specification and generates regular expression which expresses the pattern
//in:  pattern_spec -  the pattern specification 
//out: buf - allocated string with size corresponded in len_buf that will be used for new regular expression
//return: -1 - error
//	      0 - succeed
int process_pattern_spec(const char* pattern_spec,char *buf, int buf_size)
{
	pcre *re;
	const char *error;
	char *ptr;
	int erroffset;
	int ovector[OVECCOUNT];
	int pattern_spec_len;
	int rc, i, pattern_spec_off, buf_len;
	char regex_token[200];
	char all_index_patter[256];

	pattern_spec_len = (int)strlen(pattern_spec);
	pattern_spec_off=0;
	
	//check and initialize output buf for regular expression
	if( buf==0 || buf_size==0 )
		return -1;
	
	//reset buffer
	memset(buf, 0, buf_size);
	//copy prefix of regex 
	strcpy(buf, regex_start);
	buf_len = strlen(buf);

	//reset 
	memset(all_index_patter,0,sizeof(all_index_patter));

	//compile the regular expression pattern
	re = pcre_compile(regex_input_pattern, 0, &error, &erroffset, NULL);
	if (re == NULL)
	{
 		PrintLog("%s: error %s in pattern %s at offset %u\n", __FUNCTION__, error, regex_input_pattern, erroffset);
 		return -1;
	}

	// Run the matching operation 
	rc = pcre_exec(re, NULL, pattern_spec, pattern_spec_len, 0, 0, ovector, OVECCOUNT);           

	/* Matching failed: handle error cases */
	if (rc < 0)
	{
	 	switch(rc)
	 	{
	 	case PCRE_ERROR_NOMATCH: 
	 		PrintLog("%s No match\n", __FUNCTION__); 					
			break;
	 	default: 
	 		PrintLog("%s Matching error %d\n", __FUNCTION__, rc); 		
			break;
	 	}
	 	pcre_free(re);     /* Release memory used for the compiled pattern */
	 	return -1;
	}

	//next loop firstly processes result of read result from pcre_exe
	//then calls for next subsequent pcre_exec
	while(1)
	{
		/* Manual pattern specification parsing, generates token and creates regular expression */
		if( rc == 2)
		{
			ptr	= (char*)pattern_spec + ovector[0];
			ptr += 2; // skip "%{"

			//reads index of pattern
			unsigned char indx_pattern=(unsigned char)strtol(ptr,&ptr,10);
			//check that this is already occurred
			if(all_index_patter[indx_pattern]) 
			{
				// this token already was, so we use (\K<existing_index>) to match known token
				sprintf(regex_token,pattern_existen_token,indx_pattern);
			}
			else
			{
				all_index_patter[indx_pattern]=1;
				//generates token for different types
				if(*ptr=='}')
				{ // simple token
					sprintf(regex_token,pattern_S_simple_token,indx_pattern);
				}else if(*ptr=='G')
				{ // greedy token
					sprintf(regex_token,pattern_G_token,indx_pattern);
				}else if(*ptr=='S')
				{
					// S token with number of words
					ptr++;
					if( *ptr>='0' && *ptr<='9')
						i=atoi(ptr);
					else
					{
						PrintLog("%s Matching error in S token\n", __FUNCTION__);
						pcre_free(re);     /* Release memory used for the compiled pattern */
						return -1;
					}
					//first part of regex for token #S#
					sprintf(regex_token,pattern_S_multi_token[0],indx_pattern);
					//repeat middle part 
					while(i--)
						sprintf(regex_token+strlen(regex_token),pattern_S_multi_token[1]);
					//complete part
					sprintf(regex_token+strlen(regex_token),pattern_S_multi_token[2]);
				}
				else
				{
					PrintLog("%s Matching error in parsing of token\n", __FUNCTION__);
					pcre_free(re);     /* Release memory used for the compiled pattern */
					return -1;
				}
			}

			//creates the regular expression string
			strncat(buf, pattern_spec + pattern_spec_off, ovector[0] - pattern_spec_off);
			pattern_spec_off = ovector[1];
			strcat(buf,regex_token);
			regex_token[0]='\0';
		}

		for (i = 0; i < rc; i+=2 )
		{
			const char *substring_start = pattern_spec + ovector[2*i];
			int substring_length = ovector[2*i+1] - ovector[2*i];
			//avoid print non error log
			//PrintLog("%s:matched token: %.*s by offset %d\n", __FUNCTION__, substring_length, substring_start,ovector[2*i]);
		}

		/* the second and subsequent matches */
		{
			int options = 0;                 /* Normally no options */
			int start_offset = ovector[1];   /* Start at end of previous match */

			/* If the previous match was for an empty string, we are finished if we are
			at the end of the subject. Otherwise, arrange to run another match at the
			same point to see if a non-empty match can be found. */

			if (ovector[0] == ovector[1])
			{
				if (ovector[0] == pattern_spec_len) break;
				options = PCRE_NOTEMPTY_ATSTART | PCRE_ANCHORED;
			}

			/* Run the next matching operation */
			rc = pcre_exec( re, NULL, pattern_spec, pattern_spec_len, start_offset, options, ovector, OVECCOUNT);          

			if (rc == PCRE_ERROR_NOMATCH)
				break;                    /* All matches found */
			/* Other matching errors are not recoverable. */
			if (rc < 0)
			{
				PrintLog("%s Matching error %d\n", __FUNCTION__, rc); 		
				pcre_free(re);    /* Release memory used for the compiled pattern */
				return -1;
			}
		}
	}//

	//completes creation the regular expression string
	strncat(buf, pattern_spec+pattern_spec_off, pattern_spec_len-pattern_spec_off);
	strcpy(buf+(int)strlen(buf),regex_end);

	//Release memory used for the compiled pattern
	pcre_free(re);     
	return 0;
}

//function reads data from file, returns allocated buffer with data, returned pointer must be released by caller 
void* read_file(const char* fileName,unsigned* pLen)
{
	void* pRes = NULL;
	long len = 0;
	FILE* f = NULL;
	*pLen = 0;
	do
	{
		if(NULL == (f = fopen(fileName,"r"))) break;
		if(0 != fseek(f,0,SEEK_END)) break;
		if((len = ftell(f)) <= 0) break;
		if(0 != fseek(f,0,SEEK_SET)) break;
		if(NULL == (pRes = malloc(len))) break;
		if(len < (long)fread(pRes,1,len,f)) break;
		*pLen = len;
	}while(false);
	if(f) fclose(f);
	if(*pLen == 0)
	{
		if(pRes)
		{
			free(pRes);
			pRes = NULL;
		}
	}
	return pRes;
}

//
// matches data by regular expression, prints each line that is matched 
//
int match_input_data(const char*data,int data_len,const char* pattern)
{
	int result;
	pcre *re;
	const char *error;
	unsigned int option_bits;
	int erroffset;
	int crlf_is_newline;
	int ovector[OVECCOUNT];
	int rc, i;
	int utf8;

	//default result is error
	result=-1;

	// compiles the regular expression pattern
	re = pcre_compile(
		pattern,              /* the pattern */
		PCRE_MULTILINE,                    /* default options */
		&error,               /* for error message */
		&erroffset,           /* for error offset */
		NULL);                /* use default character tables */

	/* Compilation failed: print the error message and exit */
	if (re == NULL)
	{
		PrintLog("PCRE compilation failed at offset %d: %s\n", erroffset, error);
		return -1;
	}


	/*************************************************************************
	* If the compilation succeeded, we call PCRE again, in order to do a     *
	* pattern match against the subject string. This does just ONE match. If *
	* further matching is needed, it will be done below.                     *
	*************************************************************************/

	rc = pcre_exec(
		re,                   /* the compiled pattern */
		NULL,                 /* no extra data - we didn't study the pattern */
		data,              /* the subject string */
		data_len,       /* the length of the subject */
		0,                    /* start at offset 0 in the subject */
		0,                    /* default options */
		ovector,              /* output vector for substring information */
		OVECCOUNT);           /* number of elements in the output vector */

	/* Matching failed: handle error cases */
	if (rc < 0)
	{
		switch(rc)
		{
		case PCRE_ERROR_NOMATCH: 
			PrintLog("Error: %s No match: %.*s\n", __FUNCTION__, data_len,data); 					
			break;
		default: 
			PrintLog("Error: %s Matching error %d\n", __FUNCTION__, rc); 		
			break;
		}
		pcre_free(re);     /* Release memory used for the compiled pattern */
		return -1;
	}

	/* Match succeeded */
	/*************************************************************************
	* We have found the first match within the subject string. If the output *
	* vector wasn't big enough, say so. Then output any substrings that were *
	* captured.                                                              *
	*************************************************************************/

	/* The output vector wasn't big enough */
	if (rc == 0)
	{
		rc = OVECCOUNT/3;
		PrintLog("%s ovector only has room for %d captured substrings\n",__FUNCTION__, rc - 1);
	}

	/* Show matched line */
	for (i = 0; i < rc; i++)
	{
		//result will be printed in main function
// 		const char *substring_start = data + ovector[2*i];
// 		int substring_length = ovector[2*i+1] - ovector[2*i];
// 		printf("%.*s\n", substring_length, substring_start);
		result = 0;
		break; // prints only top substring
	}

	
	/* Before running the loop, check for UTF-8 and whether CRLF is a valid newline
	sequence. First, find the options with which the regex was compiled; extract
	the UTF-8 state, and mask off all but the newline options. */
	(void)pcre_fullinfo(re, NULL, PCRE_INFO_OPTIONS, &option_bits);
	utf8 = option_bits & PCRE_UTF8;
	option_bits &= PCRE_NEWLINE_CR|PCRE_NEWLINE_LF|PCRE_NEWLINE_CRLF|
		PCRE_NEWLINE_ANY|PCRE_NEWLINE_ANYCRLF;

	/* If no newline options were set, find the default newline convention from the
	build configuration. */
	if (option_bits == 0)
	{
		int d;
		(void)pcre_config(PCRE_CONFIG_NEWLINE, &d);
		/* Note that these values are always the ASCII ones, even in
		EBCDIC environments. CR = 13, NL = 10. */
		option_bits = (d == 13)? PCRE_NEWLINE_CR :
			(d == 10)? PCRE_NEWLINE_LF :
			(d == (13<<8 | 10))? PCRE_NEWLINE_CRLF :
			(d == -2)? PCRE_NEWLINE_ANYCRLF :
			(d == -1)? PCRE_NEWLINE_ANY : 0;
	}

	/* See if CRLF is a valid newline sequence. */

	crlf_is_newline =
		option_bits == PCRE_NEWLINE_ANY ||
		option_bits == PCRE_NEWLINE_CRLF ||
		option_bits == PCRE_NEWLINE_ANYCRLF;

	/* Loop for second and subsequent matches */
	for (;;)
	{
		int options = 0;                 /* Normally no options */
		int start_offset = ovector[1];   /* Start at end of previous match */

		/* If the previous match was for an empty string, we are finished if we are
		at the end of the subject. Otherwise, arrange to run another match at the
		same point to see if a non-empty match can be found. */
		if (ovector[0] == ovector[1])
		{
			if (ovector[0] == data_len) break;
			options = PCRE_NOTEMPTY_ATSTART | PCRE_ANCHORED;
		}

		/* Run the next matching operation */
		rc = pcre_exec(
			re,                   /* the compiled pattern */
			NULL,                 /* no extra data - we didn't study the pattern */
			data,              /* the subject string */
			data_len,       /* the length of the subject */
			start_offset,         /* starting offset in the subject */
			options,              /* options */
			ovector,              /* output vector for substring information */
			OVECCOUNT);           /* number of elements in the output vector */

		/* This time, a result of NOMATCH isn't an error. If the value in "options"
		is zero, it just means we have found all possible matches, so the loop ends.
		Otherwise, it means we have failed to find a non-empty-string match at a
		point where there was a previous empty-string match. In this case, we do what
		Perl does: advance the matching position by one character, and continue. We
		do this by setting the "end of previous match" offset, because that is picked
		up at the top of the loop as the point at which to start again.

		There are two complications: (a) When CRLF is a valid newline sequence, and
		the current position is just before it, advance by an extra byte. (b)
		Otherwise we must ensure that we skip an entire UTF-8 character if we are in
		UTF-8 mode. */

		if (rc == PCRE_ERROR_NOMATCH)
		{
			if (options == 0) break;                    /* All matches found */
			ovector[1] = start_offset + 1;              /* Advance one byte */
			if (crlf_is_newline &&                      /* If CRLF is newline & */
				start_offset < data_len - 1 &&    /* we are at CRLF, */
				data[start_offset] == '\r' &&
				data[start_offset + 1] == '\n')
				ovector[1] += 1;                          /* Advance by one more. */
			else if (utf8)                              /* Otherwise, ensure we */
			{                                         /* advance a whole UTF-8 */
				while (ovector[1] < data_len)       /* character. */
				{
					if ((data[ovector[1]] & 0xc0) != 0x80) break;
					ovector[1] += 1;
				}
			}
			continue;    /* Go round the loop again */
		}

		/* Other matching errors are not recoverable. */

		if (rc < 0)
		{
			PrintLog("%s Matching error %d\n",__FUNCTION__, rc);
			pcre_free(re);    /* Release memory used for the compiled pattern */
			return 1;
		}

		/* Match succeeded */

		/* The match succeeded, but the output vector wasn't big enough. */

		if (rc == 0)
		{
			rc = OVECCOUNT/3;
			PrintLog("%s ovector only has room for %d captured substrings\n",__FUNCTION__, rc - 1);
		}


		// Show matched line 
		for (i = 0; i < rc; i++)
		{
			//result will be printed in main function
// 			const char *substring_start = data + ovector[2*i];
// 			int substring_length = ovector[2*i+1] - ovector[2*i];
// 			printf("%.*s\n", substring_length, substring_start);
			result = 0;
			break; // prints only top substring
		}
	}   

	pcre_free(re);       /* Release memory used for the compiled pattern */
	return result;
}

int main(int argc, char **argv)
{
	int rc;
	char regex[1000]; // actually need to use string or something else to optimization memory

	/* processes command line that contains the pattern specification */
	
	//check input argument
	if(argc<2)
	{
		PrintLog("Error: Input argument does not have the pattern specification string\n");
		return -1;
	}

	//prepares pattern specification string, changes meta characters to literal characters 
	int ln = strlen(argv[1])+1;
	char *pattern_spec = (char*)malloc(ln*2);
	if(!pattern_spec) { PrintLog("Error: Out of memory"); return -1; }
	memcpy(pattern_spec,argv[1],ln);
	change_metacharacters(pattern_spec,ln,ln*2);

	//processes and generate regular expression by command line 
	rc = process_pattern_spec(pattern_spec,regex,sizeof(regex));
	free(pattern_spec); // free allocated memory
	if(rc<0)
	{
		PrintLog("Error: The pattern specification has not been precessed successfully\n");
		return -1;
	}
	PrintLog("RegEx=%s\n",regex);

	char *pData=0;
	unsigned len=0;

// 	if(argc==3)
// 	{
// 		pData = (char*)read_file(argv[2],&len);
// 		if(!pData) { PrintLog("Error: Out of memory"); return -1; }
// 		// execute matched to qualify each received line
// 		rc = match_input_data(pData,len,regex);	
// 	}
// 	else
	{
		//reads input stream to match by corresponded pattern
		len = 0x200; // maximum symbols in line
		pData = (char*)malloc(len);
		if(!pData) { PrintLog("Error: Out of memory"); return -1; }
		//reads all input data and keeps it
		for(;0 != fgets(pData,len,stdin);)
		{
			rc = match_input_data(pData,strlen(pData)+1,regex);
			if(rc==0)
				printf("%s",pData);
		}
	}
	
	if(pData) 
		free(pData);
	
	return 0;
}
