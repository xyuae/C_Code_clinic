/*
	crunch_data
	Written by Dan Gookin, February 17 2015

	This code reads the specific output from the `fetch_data` program
	It's assumed the input data has this format:

	2015_02_03 09:02:34 38.86  30.07   3.00
	Date, Time, Air Temperature, Barometric Pressure, Wind Speed

	That columnar data is treated as standard input. The value columns (3,4,5)
	are manipulated.  The resulting average and mean for each column are
	displayed.

	Output is in plain text. If the --json switch is specified, output is
	kludged into JSON
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_row(char *line_of_text);
void process_row(int n,char *r,float *air,float *bar,float *wind);
void set_date(char *row_string, char *date_string);
float get_mean(float *v,int c);
float get_median(float *v, int c);
int compare(const void *a, const void *b);

int main(int argc, char *argv[])
{
	char date_string[11];		/* YYYY-MM-DD */
	char row[80];
	int row_count,json_output;
	float *air_temp,*bar_press,*wind_speed;
	
	/* check for the --json argument */
	json_output = 0;
	if(argc > 1)
	{
		if( strcmp(argv[1],"--json") == 0)
			json_output = 1;
		else if( strcmp(argv[1],"--help") == 0)
		{
			puts("crunch_data\nWritten by Dan Gookin, 2015\n");
			puts("Manipulates input provided by the fetch_data program,");
			puts("generating mean and median for Air Temperature, Barometric");
			puts("Pressure, and Wind Speed. Format:\n");
			puts("crunch_data [--json] [--help]\n");
			puts("--json   Output data in JSON format");
			puts("--help   Show this message");
			return(1);
		}
		else
		{
			fprintf(stderr,"crunch_data: Unknown argument(s) ignored.\n");
		}
	}

	/* Process standard input (output from `fetch_data`) */
	row_count = 0;
	while(read_row(row))
	{
		/* This set of malloc()/realloc() operations are to dynamically allocate
		storage for the three sets of float values */
		if(row_count == 0)
		{
			set_date(row,date_string);
			air_temp = (float *)malloc(sizeof(float)*1);
			bar_press = (float *)malloc(sizeof(float)*1);
			wind_speed = (float *)malloc(sizeof(float)*1);
		}
		else
		{
			air_temp = (float *)realloc(air_temp,row_count*sizeof(float)+1);
			bar_press = (float *)realloc(bar_press,row_count*sizeof(float)+1);
			wind_speed = (float *)realloc(wind_speed,row_count*sizeof(float)+1);
			if(air_temp == NULL || bar_press == NULL || wind_speed == NULL)
			{
				fprintf(stderr,"Unable to allocate memory for data storage.\n");
				exit(1);
			}
		}
		process_row(row_count,row,air_temp,bar_press,wind_speed);
		row_count++;
	}

	/* Output results */
	if(json_output)
	{
		printf("{ \"%s\": {\n",date_string);
		printf("  \"airTemperature\": {\"mean\": %f, \"median\": %f },\n",
				get_mean(air_temp,row_count),
				get_median(air_temp,row_count));
		printf("  \"barometricPressure\": { \"mean\": %f, \"median\": %f },\n",
				get_mean(bar_press,row_count),
				get_median(bar_press,row_count));
		printf("  \"windSpeed\": { \"mean\": %f, \"median\": %f }\n",
				get_mean(wind_speed,row_count),
				get_median(wind_speed,row_count));
		printf("}\n}\n");
	}
	else	/* tabular output */
	{	
		printf("%s\n",date_string);
		printf("\tAir Temperature\n");
		printf("\t\tMean\t%f\n",get_mean(air_temp,row_count));
		printf("\t\tMedian\t%f\n",get_median(air_temp,row_count));
		printf("\tBarometric Pressure\n");
		printf("\t\tMean\t%f\n",get_mean(bar_press,row_count));
		printf("\t\tMedian\t%f\n",get_median(bar_press,row_count));
		printf("\tWind Speed\n");
		printf("\t\tMean\t%f\n",get_mean(wind_speed,row_count));
		printf("\t\tMedian\t%f\n",get_median(wind_speed,row_count));
	}

	return(0);
}

/*
	Read a line of standard input and store it in `line_of_text` buffer
	Returns characters read, which isn't used in the main() function.
*/
int read_row(char *line_of_text)
{
	char c;
	int offset = 0;
	
	while( (c=fgetc(stdin)) != EOF )
	{
		if(c=='\n')
			break;
		*(line_of_text+offset) = c;
		offset++;
	}
	return(offset);
}

/*
	Place the values read into the arrays
	
	The strtof() function is ideal for reading subsequent values in a string.
	The offset is calculated  in the first strtof(), then strtof() keeps reading
	at the next position in the string (r).
*/
void process_row(int n,char *r,float *air,float *bar,float *wind)
{
#define TABLE_OFFSET 19
	char *v2,*v3;

	*(air+n) = strtof(r+TABLE_OFFSET,&v2);
	*(bar+n) = strtof(v2,&v3);
	*(wind+n) = strtof(v3,NULL);
}

/*
	Create the data string, replacing _ with -
*/
void set_date(char *row_string, char *date_string)
{
	int x;
	char c;
	
	for(x=0;x<10;x++)
	{
		c = *(row_string+x);
		if( c == '_')
			*(date_string+x) = '-';
		else
			*(date_string+x) = c;
	}
	*(date_string+x) = '\0';
}

/*
	Calculate and return the mean (average) of the float array referenced by 'v'
*/
float get_mean(float *v, int c)
{
	int x;
	float total = 0.0;
	
	for(x=0;x<c;x++)
		total += *(v+x);
	
	return(total/c);
}

/*
	Calculate and return the media (center value) of the float array 'v'
	The array must be sorted. For an odd number of items, the middle value
	is returned. For an even number, the two middle values are averaged
	and that value is returned
*/
float get_median(float *v, int c)
{
	qsort(v,c,sizeof(float),compare);			/* quick-sort the array */
	if( c % 2)									/* test odd or even */
		return(*(v+c/2+1));						/* odd */
	else
		return( (*(v+c/2) + *(v+c/2+1)) / 2 );	/* even */
}

/*
	Comparison method used by qsort(). The order doesn't matter.
*/
int compare(const void *a, const void *b)
{
	return( *(int *)a - *(int *)b);
}

