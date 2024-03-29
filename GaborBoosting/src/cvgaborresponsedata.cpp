/***************************************************************************
 *   Copyright (C) 2007 by Mian Zhou   *
 *   M.Zhou@reading.ac.uk   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "cvgaborresponsedata.h"

namespace PrepareData {

CvGaborResponseData::CvGaborResponseData()
{
}


CvGaborResponseData::~CvGaborResponseData()
{
  clear();
}


}


/*!
    \fn PrepareData::CvGaborResponseData::CvGaborResponseData(const CvFaceDB *db, const CvPoolParams *param)
 */
 PrepareData::CvGaborResponseData::CvGaborResponseData( const CvFaceDB *db, const CvPoolParams *param)
{
	setDB( db );
	setParam( param );
	
	nRespones = nImages * nScales * nOrientations;
        generate();
}


/*!
    \fn PrepareData::CvGaborResponseData::setDB(const CvFaceDB *db)
 */
void PrepareData::CvGaborResponseData::setDB(const CvFaceDB *db)
{
  database = db->clone();
  char * dbname = database->getName();
  if( !strcmp(dbname, "XM2VTS")) 
    dbtype = XM2VTS;
  else if ( !strcmp(dbname, "FERET")) 
    dbtype = FERET;
  printf("...............a %s database copied!\n\n", dbname);
          
  if(dbtype == XM2VTS)
  {
    int nClients = ((CvXm2vts*)database)->get_num_sub();
    int imgsperClient = ((CvXm2vts*)database)->get_num_pic();
    nImages = nClients * imgsperClient;
                  
  }
  else if(dbtype == FERET)
  {
    nImages = ((CvFeret*)database)->getNum();
  }
}


/*!
    \fn PrepareData::CvGaborResponseData::setParam(const CvPoolParams *param)
 */
void PrepareData::CvGaborResponseData::setParam(const CvPoolParams *param)
{
	Orients = cvCloneMat( param->getOrients() );
	Scales = cvCloneMat( param->getScales() );
	
	CvSize size_o;
	size_o = cvGetSize( Orients );
	nOrientations = size_o.width * size_o.height;
	
	CvSize size_s;
	size_s = cvGetSize( Scales );
	nScales = size_s.width * size_s.height;
	
}



/*!
    \fn PrepareData::CvGaborResponseData::clear()
 */
void PrepareData::CvGaborResponseData::clear()
{
  delete database;
  cvReleaseMat( &Orients );
  cvReleaseMat( &Scales );
  for (int i = 0; i < nRespones ; i++)
  {
    cvReleaseImage( &(responses[i]) );
  }
	
	
}


/*!
    \fn PrepareData::CvGaborResponseData::generate()
 */
void PrepareData::CvGaborResponseData::generate()
{
  printf("Generating the response data, please waiting ....\n");
  int n = 0;
  responses = (IplImage **)cvAlloc(nRespones*sizeof(IplImage *));
  if(dbtype == XM2VTS)
  {	
    char * xm2vts_path = ((CvXm2vts*)database)->getPath();
    int nClients = ((CvXm2vts*)database)->get_num_sub();
    int imgsperClient = ((CvXm2vts*)database)->get_num_pic();
    char filename[200];
    for( int i = 1; i <= nClients; i++ )
    {
      for(int j = 1; j <= imgsperClient; j++ )
      {
        //get the images
        sprintf(filename, "%s/%d_%d.jpg", xm2vts_path, i, j);			
        IplImage *img = cvLoadImage( filename,  CV_LOAD_IMAGE_GRAYSCALE );
        for(int k = 0; k < nScales; k++)
        {
          int scale = (int)cvGetReal1D( Scales, k );
          for(int l = 0; l < nOrientations; l++)
          {
            int orientation = (int)cvGetReal1D( Orients, l);
            CvGabor gaborfilter(orientation, scale);
            responses[n] = cvCreateImage(cvSize(img->width,img->height), IPL_DEPTH_32F, 1);
            gaborfilter.conv_img((IplImage*)img, (IplImage*)responses[n], CV_GABOR_MAG);
            n++;
          }
        }	
        cvReleaseImage(&img);
      }
	//if( fmod(i,2.0) == 0.0)
	//	printf("%d\%\n",i/2);
      std::cout << i<< "\r" << std::flush;
    }
    printf("\n");
  }
  else if(dbtype == FERET)
  {
    string fapath = ((CvFeret*)database)->getFApath();
    int numsub = ((CvFeret*)database)->getFaSub();
    for(int i = 0; i < numsub; i++)
    {
      CvSubject subject = ((CvFeret*)database)->getSubject( i );
      int numpic = subject.getnum();
      for(int j = 0; j < numpic; j++)
      {
        string imgname = subject.getname(j);
        string filename = fapath + "/" + imgname;
        
        IplImage *img = cvLoadImage( filename.c_str(), CV_LOAD_IMAGE_ANYCOLOR);
  
        if(img->nChannels == 3)
        {
          IplImage *grayimg = cvCreateImage(cvGetSize(img), IPL_DEPTH_8U, 1);
          cvCvtColor( img, grayimg, CV_RGB2GRAY );
          cvReleaseImage(&img);
          img = grayimg;
        }
        
        for(int k = 0; k < nScales; k++)
        {
          int scale = (int)cvGetReal1D( Scales, k );
          for(int l = 0; l < nOrientations; l++)
          {
            int orientation = (int)cvGetReal1D( Orients, l);
            CvGabor gaborfilter(orientation, scale);
            responses[n] = cvCreateImage(cvSize(img->width,img->height), IPL_DEPTH_32F, 1);
            gaborfilter.conv_img((IplImage*)img, (IplImage*)responses[n], CV_GABOR_MAG);
            n++;
          }
        }
        cvReleaseImage(&img);
      }
      std::cout << i<< "\r" << std::flush;
    }
    printf("\n");
  }

}







/*!
    \fn PrepareData::CvGaborResponseData::loadData(const char* datapath)
 */
void PrepareData::CvGaborResponseData::loadData(const char* datapath)
{
  //check the path is valid
  DIR *pdir = NULL;
  pdir = opendir (datapath); 
  if (pdir == NULL) 
  { 
    printf ("\nERROR! %s is not a valid path!\n", datapath);
    exit (-1);
  }
  
  printf("Loading the response data, please waiting ....\n");
	
  int n = 0;
  responses = (IplImage **)cvAlloc(nRespones*sizeof(IplImage *));
  if(dbtype == XM2VTS)
  {

    char * xm2vts_path = ((CvXm2vts*)database)->getPath();
    int nClients = ((CvXm2vts*)database)->get_num_sub();
    int imgsperClient = ((CvXm2vts*)database)->get_num_pic();
    char filename[200];
    for( int i = 1; i <= nClients; i++ )
    {
      for(int j = 1; j <= imgsperClient; j++ )
      {
        for(int k = 0; k < nScales; k++)
        {
          int scale = (int)cvGetReal1D( Scales, k );
          for(int l = 0; l < nOrientations; l++)
          {
            int orientation = (int)cvGetReal1D( Orients, l);
            sprintf(filename, "%s/%d/%d/%d_%d.xml", datapath, i, j, scale, orientation);
            responses[n] = (IplImage *)cvLoad(filename,NULL,NULL,NULL);
            n++;
          }
        }
      }
      std::cout << i<< "\r" << std::flush;
    }
  }
  else if(dbtype == FERET)
  {
    struct dirent *pent;
    char filename[200];
    errno=0;
    int i = 0;
    while ((pent=readdir(pdir)))
    {
      if((strcmp(pent->d_name, "."))&&(strcmp(pent->d_name, "..")))
      {
        string name( pent->d_name );
        
        for(int k = 0; k < nScales; k++)
        {
          int scale = (int)cvGetReal1D( Scales, k );
          for(int l = 0; l < nOrientations; l++)
          {
            int orientation = (int)cvGetReal1D( Orients, l);
            sprintf( filename, "%s/%s/%d_%d.xml", datapath, name.c_str(), scale, orientation);
            responses[n] = (IplImage *)cvLoad(filename,NULL,NULL,NULL);
            n++;
          }
        }
        std::cout << i << "\r" << std::flush;
        i++;
      }
     
    }
    
    if (errno)
    {
      printf ("readdir() failure; terminating");
      exit(-1);
    }
  }

  closedir( pdir );
  printf("\n%d response images are loaded.\n", n);
}







/*!
    \fn PrepareData::CvGaborResponseData::getfeaturefrominstance(const CvGaborFeature *feature, int client_index, int picture_index) const
 */
double PrepareData::CvGaborResponseData::getfeaturefrominstance(const CvGaborFeature *feature, int client_index, int picture_index) const
{
  int scale_index = feature->getNu();
  int orientation_index = feature->getMu();
  int x = feature->getx();
  int y = feature->gety();
  int min_scale = (int)cvGetReal1D(Scales, 0);
  int min_orientation = (int)cvGetReal1D(Orients, 0);
  int nGaborResponse = nScales * nOrientations;
	
  int response_index;
  double feature_value = NAN;
  if(dbtype == XM2VTS)
  {
    int nPictureperClient = ((CvXm2vts *)database)->get_num_pic();
    response_index = (picture_index-1)*nGaborResponse + (client_index-1)*nPictureperClient + (scale_index-min_scale)*nOrientations + (orientation_index - min_orientation);
    IplImage *reponse_img = responses[response_index];
    feature_value = cvGetReal2D( reponse_img, y-1, x-1 );
  }
  else if(dbtype == FERET)
  {
    int pos = ((CvFeret*)database)->getPosFaSubjectIND(client_index);
    CvSubject subject = ((CvFeret*)database)->getFaSubject(client_index);
    int num_sub = subject.getnum();
    
    if(picture_index > num_sub)
    {
      printf("ERROR: The picture index is out of index.\n");
      exit(-1);
    } 
    
    response_index = (pos+picture_index)*nScales*nOrientations + (scale_index-min_scale)*nOrientations + (orientation_index - min_orientation);
    IplImage *reponse_img = responses[response_index];
    feature_value = cvGetReal2D( reponse_img, y-1, x-1 );
  }
	
  return feature_value;
}


/*!
    \fn PrepareData::CvGaborResponseData::getfeaturefromall(const CvGaborFeature *feature) const
 */
CvMat* PrepareData::CvGaborResponseData::getfeaturefromall(const CvGaborFeature *feature) const
{
  CvMat * Vvector = cvCreateMat(1, nImages, CV_32FC1);
  int n = 0;
  if(dbtype == XM2VTS)
  {
    int nClient = ((CvXm2vts*)database)->get_num_sub();
    int nPicturePerClient = ((CvXm2vts*)database)->get_num_pic();
    for(int i = 1; i <= nClient; i++)
    {
      for(int j = 1; j <= nPicturePerClient; j++)
      {
        double value = getfeaturefrominstance( feature, i, j );
        cvSetReal1D( Vvector, n, value );
        n++;
      } 
    }
  }
  else if(dbtype == FERET)
  {
    int numsub = ((CvFeret*)database)->getFaSub();
    for(int i = 0; i < numsub; i++)
    {
      CvSubject subject = ((CvFeret*)database)->getFaSubject(i);
      int numpic = subject.getnum();
      for(int j = 0; j < numpic; j++)
      {
        double value = getfeaturefrominstance( feature, i, j);
        cvSetReal1D( Vvector, n, value );
        n++;
      }
    }
  }
          
  return Vvector;
}




/*!
    \fn PrepareData::CvGaborResponseData::CvGaborResponseData(CvFaceDB *db, CvPoolParams *param, const char *saved_data_path)
 */
 PrepareData::CvGaborResponseData::CvGaborResponseData(CvFaceDB *db, CvPoolParams *param, const char *saved_data_path)
{
  setDB( db );
  setParam( param );
	
  nRespones = nImages * nScales * nOrientations;
  loadData( saved_data_path );
}


/*!
    \fn PrepareData::CvGaborResponseData::getDB() const
 */
CvFaceDB* PrepareData::CvGaborResponseData::getDB() const
{
  return database;
}
