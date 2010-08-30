#include "md5.h"
#include "FileSystem.h"
#include "RapidDownloader.h"
#include "RepoMaster.h"
#include "Repo.h"
#include <zlib.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include "Util.h"
#include <dirent.h>
#include <limits.h>

#ifdef WIN32
#define PATH_DELIMITER '\\'
#else
#define PATH_DELIMITER '/'
#endif


CFileSystem* CFileSystem::singleton = NULL;

bool CFileSystem::fileIsValid(const FileData* mod, const std::string& filename) const{
	MD5_CTX mdContext;
	int bytes;
	unsigned char data[1024];
	struct stat sb;
	if (stat(filename.c_str(),&sb)<0){
		return false;
	}
	if (sb.st_size!=mod->size){
		printf("File %s invalid, size wrong: %d but should be %d\n", filename.c_str(),sb.st_size, mod->size);
		return false;
	}
	if (!sb.st_mode&S_IFREG){
		printf("File is no file %s\n", filename.c_str());
		return false;
	}

	gzFile inFile = gzopen (filename.c_str(), "rb");
	if (inFile == NULL) { //file can't be opened
		return false;
	}
	MD5Init (&mdContext);
	while ((bytes = gzread (inFile, data, 1024)) > 0)
		MD5Update (&mdContext, data, bytes);
	MD5Final (&mdContext);
	gzclose (inFile);
	int i;
	for(i=0; i<16;i++){
		if (mdContext.digest[i]!=mod->md5[i]){ //file is invalid
//			printf("Damaged file found: %s\n",filename.c_str());
//			unlink(filename.c_str());
			return false;
		}
	}
	printf("Valid file found: %s\n",filename.c_str());
	return true;
}

/*
	parses the file for a mod and creates
*/
bool CFileSystem::parseSdp(const std::string& filename, std::list<CFileSystem::FileData*>& files){
	char c_name[255];
	unsigned char c_md5[16];
	unsigned char c_crc32[4];
	unsigned char c_size[4];

	gzFile in=gzopen(filename.c_str(), "r");
	printf("parse_binary: %s\n",filename.c_str());
	if (in==Z_NULL){
        printf("Could not open %s\n",filename.c_str());
		return NULL;
	}
	files.clear();
	FileData tmpfile;
	while(!gzeof(in)){
		int length = gzgetc(in);
		if (length == -1) break;
		if (!gzread(in, &c_name, length)) return false;
		if (!gzread(in, &c_md5, 16)) return false;
		if (!gzread(in, &c_crc32, 4)) return false;
		if (!gzread(in, &c_size, 4)) return false;

		FileData *f = new FileData;
		f->name = std::string(c_name, length);
		std::memcpy(&f->md5, &c_md5, 16);
		f->crc32 = parse_int32(c_crc32);
		f->size = parse_int32(c_size);
		files.push_back(f);
	}
	return true;
}



std::string CFileSystem::createTempFile(){
	std::string tmp;
#ifndef WIN32
	tmp=tmpnam(NULL);
#else
	char buf[MAX_PATH];
	char tmppath[MAX_PATH];
	GetTempPath(sizeof(tmppath),tmppath);
	GetTempFileName(tmppath,NULL,0,buf);
	tmp.assign(buf);
#endif
	tmpfiles.push_back(tmp);
	return tmp;
}

CFileSystem::~CFileSystem(){
	std::list<std::string>::iterator it;
	for (it = tmpfiles.begin();it != tmpfiles.end(); ++it) {
		remove(it->c_str());
	}
	tmpfiles.clear();
}

CFileSystem::CFileSystem(){
	tmpfiles.clear();
	springdir="/home/matze/.spring";
}

void CFileSystem::Initialize(){
	singleton=new CFileSystem();
}

void CFileSystem::Shutdown(){
	CFileSystem* tmpFileSystem=singleton;
	singleton=NULL;
	delete tmpFileSystem;
	tmpFileSystem=NULL;
}

const std::string& CFileSystem::getSpringDir() const{
	return springdir;
}

/**
	checks if a directory exists
*/
bool CFileSystem::directory_exist(const std::string& path) const{
	struct stat fileinfo;
	int res=stat(path.c_str(),&fileinfo);
	return (res==0);
}

/**
	creates a directory with all subdirectorys (doesn't handle c:\ ...)
*/
void CFileSystem::create_subdirs (const std::string& path) const {
	bool run=false;
	for (unsigned int i=0;i<path.size(); i++){
		char c=path.at(i);
		if (c==PATH_DELIMITER){
#ifdef WIN32
			mkdir(path.substr(0,i).c_str());
#else
			mkdir(path.substr(0,i).c_str(),0777);
#endif
			run=true;
		}
	}
#ifdef WIN32
	mkdir(path.c_str());
#else
	mkdir(path.c_str(),0777);
#endif
}


const std::string CFileSystem::getPoolFileName(CFileSystem::FileData* fdata) const{
	std::string name;
	std::string md5;

	md5ItoA(fdata->md5, md5);
	name=getSpringDir();
	name += "/pool/";
	name += md5.at(0);
	name += md5.at(1);
	name += "/";
	create_subdirs(name);
	name += md5.substr(2);
	name += ".gz";
	return name;
}

/**
	Validate all files in /pool/ (check md5)
*/
void CFileSystem::validatePool(const std::string& path){
	DIR* d;
	d=opendir(path.c_str());

	if (d!=NULL){
		struct dirent* dentry;
		while( (dentry=readdir(d))!=NULL){
			struct stat sb;
			std::string tmp;
			if (dentry->d_name[0]=='.')
				break;
			tmp=path+"/"+dentry->d_name;
			stat(tmp.c_str(),&sb);
			if(sb.st_mode&S_IFDIR!=0){
				validatePool(tmp);
			}else{
				printf("%s\n",tmp.c_str());
			}

		}

	}

}
