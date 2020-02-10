#ifndef INIFILE_H
#define INIFILE_H

/*<! basic data type re-definition */
#ifndef _SE_DATA_TYPE___
#define _SE_DATA_TYPE___

typedef unsigned char seByte, *sePByte;

typedef struct _seFloat2D{
    float x;
    float y;
}seFloat2D;

typedef struct _seFloat3D{
    float x;
    float y;
    float z;
}seFloat3D;

typedef struct _seRectF {
    float l;
    float t;
    float r;
    float b;
}seRectF;
#endif //_SE_DATA_TYPE___


void* openIniFile(const char* iniFilename, bool readOnly=false);
bool saveAsIniFile(void* handle, const char* iniName);
void closeIniFile(void* handle);
bool GetProfileString( const char *section, const char *key,char *value, int size,const char *default_value, void * handle);
bool WriteProfileString( const char *section, const char *key,const char *value, void *handle);
int GetProfileInt( const char *section, const char *key,int default_value, void * handle);
bool WriteProfileInt( const char * lpSecName,const char * lpKeyName, int value, void * handle);
long int GetProfileHex(const char *section, const char *key,long int default_value, void * handle);

/*<! *************************     extented Profile functions      *************************************/
float GetProfileFloat(const char *lpSecName, const char *lpKeyName, float fDefault,  void * handle);
bool WriteProfileFloat(  const char *lpSecName, const char * lpKeyName, float value,  void * handle);

double GetProfileDouble(const char *lpSecName, const char *lpKeyName, double dbDefault,  void * handle);
bool WriteProfileDouble(  const char *lpSecName, const char * lpKeyName, double value,  void * handle);

bool WriteProfilePointFloat(const char *lpSecName, const char *lpKeyName,  seFloat2D *pPoint, void * handle);
bool GetProfilePointFloat(const char * lpSecName, const char * lpKeyName,  seFloat2D* pPoint, void * handle);

bool WriteProfileArrayFloat(const char * lpSecName, const char * lpKeyName,  float* pValue, int nElements, void * handle);
bool GetProfileArrayFloat(const char * lpSecName, const char * lpKeyName,   float* pValue, int nElements,  void * handle);
bool	WriteProfileRectFloat(const char * lpSecName, const char * lpKeyName,  seRectF* pRect, void * handle);
bool GetProfileRectFloat(const char * lpSecName, const char * lpKeyName,  seRectF* pRect, void * handle);
bool WriteProfileArrayInt(const char * lpSecName, const char * lpKeyName,  int* pValue, int nElements, void * handle);
bool GetProfileArrayInt(const char * lpSecName, const char * lpKeyName,   int* pValue, int nElements,  void * handle);


#endif // INIFILE_H

