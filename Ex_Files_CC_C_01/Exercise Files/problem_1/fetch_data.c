/*
	fetch_data
	Written by Dan Gookin, Feburary 17, 2015

	This code reads raw data from three websites. Current data is read,
	unless a specific date is used as an argument, format YYYYMMDD.

	Data is fetched by using the curl library; compile with -lcurl
		http://curl.haxx.se/libcurl/
	
	The code stores the data in memory, then merges the three tables
	(or pages) into a single table. That table is output in a five column,
	space-separated format:

	2015_02_03 09:02:34 38.86  30.07   3.00
	Date, Time, Air Temperature, Barometric Pressure, Wind Speed
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <curl/curl.h>

#define VALUE_READ_OFFSET 19
#define DATE_STRING_SEPARATOR '_'

struct web_data {
	char *buffer;
	size_t size;
};

void set_address_date(char *address, char *year, char *date);
long fetch_web_data(struct web_data *chunk, char *address);
static size_t write_mem(void *contents, size_t size, size_t n, void *userp);
int write_line(char *text,int offset);
void show_help(void);

int main(int argc, char *argv[])
{
	struct web_data air_temp,barometric_press,wind_speed;
	int a,b,output,written;
	long bytes_read;
	time_t tictoc;
	struct tm *date;
	char year[5],datestring[11];
	/* check set_address_date() for offsets used in these addresses */
	char air_temp_address[] = "http://lpo.dt.navy.mil/data/DM/2014/2014_01_01/Air_Temp";
	char barometric_press_address[] = "http://lpo.dt.navy.mil/data/DM/2014/2014_01_01/Barometric_Press";
	char wind_speed_address[] = "http://lpo.dt.navy.mil/data/DM/2014/2014_01_01/Wind_Speed";

	/* Read command line parameters */
	if(argc <= 1)
	{
			/* no arguments specified, use today's date */
		time(&tictoc);
		date = localtime(&tictoc);
		sprintf(year,"%4d",date->tm_year+1900);
		sprintf(datestring,"%4d_%02d_%02d",date->tm_year+1900,date->tm_mon+1,date->tm_mday);
	}
	else
	{
		if(strcmp(argv[1],"--help")==0)
		{
			show_help();
			exit(0);
		}
		else		/* a date is specified */
		{
			for(a=0;a<8;a++)
			{
				if(!isdigit(argv[1][a]))
				{
					fprintf(stderr,"Improper date format: Use YYYYMMDD\n");
					exit(1);
				}
			}
			/* extract year from date */
			for(a=0;a<4;a++)
				year[a] = argv[1][a];
			year[a] = '\0';
			/* manipulate date string into web page address format: YYYY_MM_DD */
			for(a=0,b=0;a<10;a++,b++)
			{
				if(a==4 || a==7)
					datestring[a++] = DATE_STRING_SEPARATOR;
				datestring[a] = argv[1][b];
			}
			datestring[a] = '\0';
		}
	}
	/* patch the web page addresses to reflect the desired date */
	set_address_date(air_temp_address,year,datestring);
	set_address_date(barometric_press_address,year,datestring);
	set_address_date(wind_speed_address,year,datestring);

	/*
		Read the web pages and store the data. The value for `bytes_read`
		is the same for each page, so it's stored only once */
	bytes_read = fetch_web_data(&air_temp,air_temp_address);
	/*
		Check for error on the first call. All three pages would be
		down together */
	if( strstr(air_temp.buffer,"error.html")!=NULL)
	{
		fprintf(stderr,"Web page error reported.\nConfirm correct date.\n");
		exit(1);
	}
	fetch_web_data(&barometric_press,barometric_press_address);
	fetch_web_data(&wind_speed,wind_speed_address);

	/* output data in 3 column format */
	/*
		`output` keeps track of text output, which balances
		`bytes_read` for input. Because `write_line` for the
		`air_temp.buffer` outputs the same length as input, the
		numbers track equally, fully dumping all the data in the
		desired format. */
	output = 0;
	while(output < bytes_read)
	{
		written = write_line(air_temp.buffer+output,0);
		putchar(' ');
		write_line(barometric_press.buffer+output,VALUE_READ_OFFSET);
		putchar(' ');
		write_line(wind_speed.buffer+output,VALUE_READ_OFFSET);
		putchar('\n');
		output += written;
	}

	/* release memory chunks */
	if(air_temp.buffer) free(air_temp.buffer);
	if(barometric_press.buffer) free(barometric_press.buffer);
	if(wind_speed.buffer) free(wind_speed.buffer);

	return(0);
}

/*
   Assign the proper date to the web page address string
*/
void set_address_date(char *address, char *year, char *date)
{
#define YEAR_OFFSET 31
#define DATE_OFFSET 36
#define YEAR_SIZE 4
#define DATE_SIZE 10

	int x;
	for(x=0;x<YEAR_SIZE;x++)
		*(address+YEAR_OFFSET+x) = *(year+x);
	for(x=0;x<DATE_SIZE;x++)
		*(address+DATE_OFFSET+x) = *(date+x);
}

/*
	Fill the web data buffer with text read from the web page
 */
long fetch_web_data(struct web_data *chunk, char *address)
{
	CURL *curl;
	CURLcode res;

	chunk->buffer = malloc(1);
	chunk->size = 0;
	
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if(!curl)
	{
		fprintf(stderr,"Unable to initialize curl.\n");
		exit(1);
	}

	/* configure libcurl to read and store the information */
	curl_easy_setopt(curl, CURLOPT_URL, address);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_mem);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

	res = curl_easy_perform(curl);
	if( res != CURLE_OK)
	{
		fprintf(stderr,"curl failed: %s\n",curl_easy_strerror(res));
		exit(1);
	}

	/* clean up */
	curl_easy_cleanup(curl);
	
	return((long)chunk->size);
}

/*
   Routine used by libcurl to store web page data into a buffer
   `ptr` = delivered data
   `size` = size of chunk
   `nmemb` = number of chunks
   `userdata` = storage, set by CALLOPT_WRITEDATA (`chunk` in this code )
*/
static size_t write_mem(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t realsize;
	struct web_data *mem;

	realsize = size * nmemb;
	mem = (struct web_data *)userdata;
	
	/* re-size the input buffer to accomodate the information read */
	mem->buffer = realloc(mem->buffer, mem->size + realsize + 1);
	if(mem->buffer == NULL)
	{
		fprintf(stderr,"Unable to allocate buffer for web page storage.\n");
		exit(1);
	}

	memcpy(&(mem->buffer[mem->size]),ptr,realsize);
	mem->size += realsize;
	mem->buffer[mem->size] = 0;

	return(realsize);
}

/*
   output a line of text at a given offset, no \n
   skip over the CR/LF combo at the end of the line
   return the number of characters written
*/
int write_line(char *text,int offset)
{
	char *temp;
	
	temp = text;
	text += offset;
	while(isprint(*text))
	{
		putchar(*text);
		text++;
	}
	text+=2;		/* skip over 0x0d and 0x0a at the end of each line */
	
	return(text-temp);
}

/*
   	Output help/about message
*/
void show_help(void)
{
	puts("fetch_data\nWritten by Dan Gookin, 2015\n");
	puts("Fetches air temperature, barometric pressure, and wind speed data from");
	puts("http://lpo.dt.navy.mil/, Acoustic Research Dept. Lake Pend Oreille, ID\n");
	puts("No options: Fetch current day's data (results may be incomplete)");
	puts("YYYYMMDD    Fetch data for given date");
	puts("--help      Show this message\n");
	puts("Output is in the format: Date Time Air_temp Bar_press Wind_speed");
}


