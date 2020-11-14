#include <assert.h>
#include <dirent.h> 
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERBOSE 0

#if VERBOSE >= 2
#define PRINT_NOTES()\
puts("------------------");\
printf(". Note: %s", line);\
printf("%lf / %lf\n", note_ratio->numerator, note_ratio->denominator);
#else
#define PRINT_NOTES()
#endif

#if VERBOSE >= 1
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

static const char* dirName = "./scales/scl/";
#define necro_scale_name_size 128

int parseFile(size_t dirSize, const char* fileName, FILE* necroFile)
{
  assert(fileName && fileName[0]);
  size_t filePathSize = strlen(fileName) + dirSize + 2;
  char* filePath = malloc(filePathSize);   

  { // initialize file path
    assert(filePath);
    memset(filePath, 0, filePathSize);
    strcpy(filePath, dirName);
    strcat(filePath, fileName);
    LOG("Parsing File Path: %s\n", filePath);
  }

  { // read file
    size_t numFiles = 0;
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    size_t read;
    int errnum = 0;

    fp = fopen(filePath, "r");
    if (fp == NULL)
    {
      printf("File open failed! %s\n", filePath);
      errnum = errno;
      fprintf(stderr, "Value of errno: %d\n", errno);
      perror("Error printed by perror");
      fprintf(stderr, "Error opening file: %s\n", strerror(errnum));
      return 1;
    }

    enum
    {
      PARSE_DESCRIPTION,
      PARSE_NUMBER_NOTES,
      PARSE_NOTES,
      PARSE_ERROR
    } parseState = PARSE_DESCRIPTION;

    typedef struct
    {
      double numerator;
      double denominator;
    } note_ratio;

    struct
    {
      int numNotes;
      note_ratio* ratios;
      char* description; // comment added to scale in necro
    } parsed_scale = { 0, NULL, NULL };

    int currentNote = 0;

    while ((read = getline(&line, &len, fp)) != -1 && parseState != PARSE_ERROR)
    {
      if (line[0] == 0 || line[0] == '!')
        continue;

      // parse file contents
      switch(parseState)
      {
        case PARSE_DESCRIPTION:
          {
            assert(parsed_scale.description == NULL);
            size_t len = strlen(line);
            if (len > 0)
            {
              parsed_scale.description = malloc(len + 1);
              strcpy(parsed_scale.description, line);
              for (size_t i = 0; i < len; ++i)
              {
                if (parsed_scale.description[i] == '\r')
                  parsed_scale.description[i] = ' ';
              }
              LOG("Description: %s\n", parsed_scale.description);
            }
            parseState = PARSE_NUMBER_NOTES;
          }
          break;

        case PARSE_NUMBER_NOTES:
          LOG("Num Notes line: %s", line);
          sscanf(line, "%i", &parsed_scale.numNotes);
          if (parsed_scale.numNotes > 0)
          {
            parsed_scale.numNotes++; // add 1/1 note
            size_t ratiosSize = sizeof(note_ratio) * (parsed_scale.numNotes + 1);
            parsed_scale.ratios = malloc(ratiosSize);
            memset(parsed_scale.ratios, 0, ratiosSize);
            parsed_scale.ratios[0] = (note_ratio) { 1.0, 1.0 };
            ++currentNote;
          }
          LOG("Number of notes: %i\n", parsed_scale.numNotes);
          parseState = PARSE_NOTES;
          break;

        case PARSE_NOTES:
          {
            assert(parsed_scale.ratios != NULL);
            assert(currentNote < parsed_scale.numNotes);
            note_ratio* note_ratio = parsed_scale.ratios + currentNote;
            const char* divider = NULL;
            if (divider = strchr(line, '.'), divider != NULL)
            {
              double cents = 0.0;
              if (sscanf(line, "%lf\n", &cents) != EOF)
              {
                note_ratio->numerator = pow(2.0, cents / 1200.0);
                note_ratio->denominator = 1.0;
                PRINT_NOTES();
              }
              else
              {
                puts("cents parse error!");
                parseState = PARSE_ERROR; 
              }
            }
            else if (divider = strchr(line, '/'), divider != NULL)
            {
              if (sscanf(line, "%lf/%lf\n", &note_ratio->numerator, &note_ratio->denominator) != EOF)
              {
                PRINT_NOTES();
              }
              else
              {
                puts("ratio parse error!");
                parseState = PARSE_ERROR; 
              }
            }
            else if (sscanf(line, "%lf\n", &note_ratio->numerator) != EOF)
            {
              note_ratio->denominator = 1.0;
              PRINT_NOTES();
            }
            else
            {
              printf("Parsing File Path: %s\n", filePath);
              printf("Undefined Note!: %s\n", line);
              parseState = PARSE_ERROR; 
            }
            ++currentNote;
          }
          break;

        case PARSE_ERROR:
          printf("Parsing File Path: %s\n", filePath);
          puts("PARSE ERROR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
          assert(0);
          break;

        default:
          assert(0 && "unexpected parse state!");
          break;
      }
    }

    if (parsed_scale.numNotes > 0 && parseState != PARSE_ERROR)
    {
      assert(currentNote == parsed_scale.numNotes);
      assert(parsed_scale.ratios != NULL);

      char necro_scale_name[necro_scale_name_size] = { 0 };
      char necro_code[16384] = { 0 };
      char necro_tuning_code[16384] = { 0 };
      char necro_scale_code[4096] = { 0 };
      size_t offset = 0;
      if (fileName[0] < 'a')
      {
        const char* header = "scala_";
        offset = strlen(header);
        strcpy(necro_scale_name, header);
      }

      { // Character fixup
        size_t i = 0;
        for (; fileName[i] && fileName[i] != '.'; ++i)
        {
          size_t necroIndex = i + offset;
          assert(necroIndex < necro_scale_name_size);
          necro_scale_name[necroIndex] = fileName[i];
          switch (necro_scale_name[necroIndex])
          {
            case ' ':
            case '-':
            case '(':
            case ')':
              necro_scale_name[necroIndex] = '_';
              break;
            case '#':
              strcpy(necro_scale_name + necroIndex, "Sharp");
              offset += strlen("Sharp");
              break;
            case '+':
              strcpy(necro_scale_name + necroIndex, "_plus_");
              offset += strlen("_plus_");
              break;
          }
        }
      }

      {
        // Manual name fixup
        if (strcmp(necro_scale_name, "sin") == 0)
        {
          strcat(necro_scale_name, "Scale");
        }
      }

      { // generate tuning code
        char* necro_tuning_code_tuning = necro_tuning_code;
        for (size_t tuning_index = 0; tuning_index < parsed_scale.numNotes; ++tuning_index)
        {
          const char* ratio_format = "%lf/%lf";
          const char* float_format = "%lf";
          const char* format = ratio_format;
          char tuning_string[256] = { 0 };
          double numerator = parsed_scale.ratios[tuning_index].numerator;
          double denominator = parsed_scale.ratios[tuning_index].denominator;

          if (denominator == 1.0)
          {
            sprintf(tuning_string, "%.17G", numerator);
          }
          else
          {
            sprintf(tuning_string, "%lu/%lu", (uint64_t) numerator, (uint64_t) denominator);
          }

          if (tuning_index + 1 < parsed_scale.numNotes)
          {
            necro_tuning_code_tuning += sprintf(necro_tuning_code_tuning, "%s, ", tuning_string);
          }
          else
          {
            necro_tuning_code_tuning += sprintf(necro_tuning_code_tuning, "%s", tuning_string);
          }
        }
      }

      { // generate scale degree code
        char* necro_scale_code_degree = necro_scale_code;
        const size_t lastDegree = parsed_scale.numNotes - 1;
        for (size_t degree = 0; degree < lastDegree; ++degree)
        {
          if (degree + 1 < lastDegree)
          {
            necro_scale_code_degree += sprintf(necro_scale_code_degree, "%zu, ", degree);
          }
          else
          {
            necro_scale_code_degree += sprintf(necro_scale_code_degree, "%zu", degree);
          }
        }
      }

      assert(parsed_scale.numNotes > 0);
      sprintf(
        necro_code,
        "----------------------------------------------------------------------\n"
        "-- %s\n"
        "-- %s\n"
        "%s :: Float -> Scale %u %u\n"
        "%s rootFreq =\n"
        "  Scale\n"
        "    { %s }\n"
        "    { %s }\n"
        "    rootFreq\n\n",
        necro_scale_name,
        parsed_scale.description,
        necro_scale_name,
        parsed_scale.numNotes,
        parsed_scale.numNotes - 1,
        necro_scale_name,
        necro_tuning_code,
        necro_scale_code
      );

      LOG("%s\n", necro_code);
      fputs(necro_code, necroFile);
    }

    if (parsed_scale.ratios != NULL)
    {
      free(parsed_scale.ratios);
    }

    if (parsed_scale.description != NULL)
    {
      free(parsed_scale.description);
    }

    fclose(fp);
  }

  free(filePath);
  return 0;
}

const char* headerComment =
"-------------------------------------------------------------------------------------------------------------------\n"
"-------------------------------------------------------------------------------------------------------------------\n"
"-- Scales and tunings from the Scala library, generated by an programmatic translation of .scl files               \n"
"-- more information on the Scala project can be found here:                                                        \n"
"-- http://www.huygens-fokker.org/scala/                                                                            \n"
"-------------------------------------------------------------------------------------------------------------------\n"
"-------------------------------------------------------------------------------------------------------------------\n\n";

int main()
{
  const size_t dirSize = strlen(dirName);
  DIR* d;
  struct dirent* dir;
  d = opendir(dirName);
  FILE* necroFile = fopen("./scales.necro", "w");
  if (necroFile == NULL)
  {
    puts("Error opening scales.necro!");
    return 1;
  }

  struct dirent **namelist;
  const int n = scandir(dirName, &namelist, 0, alphasort);
  if (n < 0)
    perror("scandir");
  else
  {
    fputs(headerComment, necroFile);
    for (int i = 0; i < n; ++i)
    {
      puts(namelist[i]->d_name);
      parseFile(dirSize, namelist[i]->d_name, necroFile);
      free(namelist[i]);
    }
    free(namelist);
  }

  fclose(necroFile);
  return 0;
}
