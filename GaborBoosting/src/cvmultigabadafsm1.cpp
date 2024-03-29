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
#include "cvmultigabadafsm1.h"

namespace MultiAdaGabor {

CvMultiGabAdaFSM1::CvMultiGabAdaFSM1()
{
}


CvMultiGabAdaFSM1::~CvMultiGabAdaFSM1()
{
  clear();
}


}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::CvMultiGabAdaFSM1(CvFaceDB* facedb, CvPoolParams* params)
 */
 MultiAdaGabor::CvMultiGabAdaFSM1::CvMultiGabAdaFSM1(CvFaceDB* facedb, CvPoolParams* params)
{
  setDB( facedb );
  setPool( params );
  init_weights();
  knn = new CvKNearest;
  svm = new CvSVM;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::setPool(CvPoolParams *param)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::setPool(CvPoolParams *param)
{
  old_pool = new CvGaborFeaturePool;
  old_pool->Init( param);
  printf(".................a %d long pool built!\n", old_pool->getSize());
  new_pool = new CvGaborFeaturePool;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::setDB(CvFaceDB *facedb)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::setDB(CvFaceDB *facedb)
{
  database = facedb->clone();
  char * dbname = database->getName();
  if( !strcmp(dbname, "XM2VTS")) this->db_type = XM2VTS;
  else if ( !strcmp(dbname, "FERET")) this->db_type = FERET;
  printf("...............a %s database copied!\n\n", dbname);
  
  if(db_type == XM2VTS)
  {
    nClass = ((CvXm2vts*)database)->get_num_sub();
    nperClass = ((CvXm2vts*)database)->get_num_pic();
    nsamples = nClass*nperClass;
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::init_weights()
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::init_weights()
{
  this->weights = cvCreateMat(nsamples, 1, CV_64FC1);
  double v = 1.0/(double)nsamples;
  for(int i = 0; i < nsamples; i++)
  {
    cvSetReal1D(weights, i, v);
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::train(int nfeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::train(int nfeatures)
{
  nexpfeatures = nfeatures;
  
 
  struct stat dummy;
  if (stat( "feature_0.txt", &dummy) == 0)
  {
    resume();
  }
  else
  {
    for(int i = 0; i < nexpfeatures; i++)
    {
      this->current = i;
      normalise( weights );
      trainallfeature(old_pool);
      findminerror( old_pool);
      update( weights );
    }
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::normalise(CvMat *mat)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::normalise(CvMat *mat)
{
  int num = nsamples;
  double v ;
  double sum = 0.0;
  for (int i = 0; i < num; i++)
  {
    v = cvGetReal1D(mat, i);
    sum = sum + v;
  }
  for (int i = 0; i < num; i++)
  {
    v = cvGetReal1D(mat, i);
    cvSetReal1D(mat, i, v/sum);
  }
  char *weightfilename = new char[20];
  sprintf(weightfilename, "weight_%d.xml", current); 
  writeweights( weightfilename, mat);
  delete [] weightfilename;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::trainallfeature(CvGaborFeaturePool *pool)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::trainallfeature(CvGaborFeaturePool *pool)
{
  char *featurefilename = new char[20];
  sprintf(featurefilename, "feature_%d.txt", current); 
  for (int i = 0; i < pool->getSize(); i++)
  {
    CvGaborFeature *feature;
    feature = pool->getfeature(i);
    double t_error = featureweak( feature );
    
    writefeature( featurefilename, feature, t_error, 0.0);
  }
  delete [] featurefilename;
}



/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::featureweak(CvGaborFeature* feature)
 */
double MultiAdaGabor::CvMultiGabAdaFSM1::featureweak(CvGaborFeature* feature)
{
  time_t start,end;
  double dif;
  time (&start);
  
  printf("Learning a weak classifier based on a Gabor Feature ( %d, %d, %d, %d) ...........\n", 
         feature->getx(), feature->gety(), feature->getNu(), feature->getMu());
  CvTrainingData *data = feature->_XM2VTSMulti_F( (CvXm2vts*)database );
  data->setweights(weights);
  CvMWeakLearner *mweak = new CvMWeakLearner( (CvXm2vts*)database, CvWeakLearner::POTSU );
  mweak->train( data );

  double e = mweak->get_error();
  printf("The error of this weak classifier is %f\n\n", e);
  feature->seterror(e);
  
  
  time (&end);
  dif = difftime (end,start);
  printf("It took %.2lf seconds \n\n", dif);
  
  delete data;
  delete mweak;
  return e;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::findminerror(CvGaborFeaturePool *pool)
 */
double MultiAdaGabor::CvMultiGabAdaFSM1::findminerror(CvGaborFeaturePool *pool)
{
  double merror;
  if (pool->getSize() < 1)
  {
    return -1.0;
  }
  else
  { 
    CvGaborFeature *sfeature;
    sfeature = pool->min();
    merror = sfeature->geterror();
    signfeature(sfeature);
    
    return merror;
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::signfeature(CvGaborFeature *feature)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::signfeature(CvGaborFeature *feature)
{
  new_pool->add(feature);
  delete feature;
  nselecfeatures++;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::update(CvMat* mat)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::update(CvMat* mat)
{
  if (old_pool->getSize() >= 1)
  {
    double error, alpha;
    CvGaborFeature *feature;
    feature = new_pool->getfeature(current);

    CvTrainingData *data;

    data = feature->_XM2VTSMulti_F((CvXm2vts*)database);
    data->setweights(weights);
    
    CvMWeakLearner *mweak = new CvMWeakLearner( (CvXm2vts*)database, CvWeakLearner::POTSU );
    mweak->train(data);
  
    for(int i = 0; i < nsamples; i++)
    {
      double weight = data->getweightofsample( i );
      cvSetReal1D(mat, i, weight);
    }
    
    error = mweak->get_error();
    alpha = log((1-error)/error)/2;
    writefeature( "signficant.txt", feature, error, alpha );
    
    printf("Push a multi-class weaklearner .......\n");
    mweaks.push_back(*mweak);
    alphas.push_back(alpha);
    
    delete data;
  }
}





/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::writefeature(const char* filename, CvGaborFeature *feature, double error, double alpha)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::writefeature(const char* filename, CvGaborFeature *feature, double error, double alpha)
{
  FILE * file;
  file = fopen ( filename, "a" );
  
  int x = feature->getx();
  int y = feature->gety();
  int Mu = feature->getMu();
  int Nu = feature->getNu();
  
  fprintf (file, "%d %d %d %d %f %f\n",x, y, Nu, Mu, error, alpha);
  
  fclose(file);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::writeweights(const char *filename, CvMat *mat)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::writeweights(const char *filename, CvMat *mat)
{
  cvSave( filename, (CvMat*)mat, NULL, NULL); 
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::clear()
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::clear()
{
  alphas.clear();
  mweaks.clear();
  delete database;
  delete new_pool;
  delete old_pool;
  delete knn;
  delete svm;
  cvReleaseMat(&weights);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::resume()
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::resume()
{
  int n = 0;
  char *featurefilename = new char[20];
  char *weightfilename = new char[20];
  struct stat dummy;
  while(1)
  {
    sprintf(featurefilename, "feature_%d.txt", n);
    sprintf(weightfilename, "weight_%d.txt", n);
    if ((stat(featurefilename, &dummy) != 0)&&(stat(weightfilename, &dummy) != 0)) break;
    n++;
  }
  delete [] featurefilename;
  delete [] weightfilename;
  int c = n - 1;
  if( c == 0 )
  {
  }
  else
  {
    old_pool->clear();
    cvReleaseMat(&weights);
    old_pool = new CvGaborFeaturePool;
    int tn = c - 1;
    char *featurefilename = new char[20];
    char *weightfilename = new char[20];
    sprintf(featurefilename, "feature_%d.txt", tn);
    sprintf(weightfilename, "weight_%d.xml", tn+1);
    old_pool->load(featurefilename);
    weights = (CvMat*)cvLoad( weightfilename, NULL, NULL, NULL );
    delete [] featurefilename;
    delete [] weightfilename;
  } 
  current = c;
  resume(c);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::resume(int n)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::resume(int n)
{
  FILE * file;
  char *featurefilename = new char[20];
  sprintf(featurefilename, "feature_%d.txt", n);
  if ((file=fopen( featurefilename, "r" )) == NULL)
  {
    printf("Cannot read file %s.\n", featurefilename);
    exit(1);
  }
  printf("Loading features from the log file %s", featurefilename);
  int x,y,Mu,Nu;
  float error;
  float alpha;
  
  int m = 0;
  int i;
  while (!feof(file))
  {
    if (fscanf(file, "%d ", &x) == EOF) break;
    if (fscanf(file, "%d ", &y) == EOF) break;
    if (fscanf(file, "%d ", &Nu) == EOF) break;
    if (fscanf(file, "%d ", &Mu) == EOF) break;
    if (fscanf(file, "%f ", &error) == EOF) break;
    if (fscanf(file, "%f\n", &alpha) == EOF) break;
    CvGaborFeature *feature = old_pool->getfeature( m );

    
    if((feature->getx()==x)&&(feature->gety()==y)&&(feature->getMu()==Mu)&&(feature->getNu()==Nu))
    {
      feature->seterror( error );
      m++;
    }
    else break;
  }
  printf("    ... done!\n");
  fclose(file);
  delete [] featurefilename;
  continue_training( m, current );
}



/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::continue_training(int no, int current)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::continue_training(int no, int current)
{
  
  char *featurefilename = new char[20];
  sprintf(featurefilename, "feature_%d.txt", current); 
  for (int i = no; i < old_pool->getSize(); i++)
  {
    CvGaborFeature *feature;
    feature = old_pool->getfeature(i);
    double t_error = featureweak( feature );
    
    writefeature( featurefilename, feature, t_error, 0.0);
  }
  delete [] featurefilename;

  
  current++;
  for(int i = current; i < nexpfeatures; i++)
  {
    this->current = i;
    normalise( weights );
    trainallfeature(old_pool);
    findminerror( old_pool);
    update( weights );
  }
  
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::CvMultiGabAdaFSM1(CvFaceDB* facedb, CvGaborFeaturePool* pool)
 */
 MultiAdaGabor::CvMultiGabAdaFSM1::CvMultiGabAdaFSM1(CvFaceDB* facedb, CvGaborFeaturePool* pool)
{
  setDB( facedb );
  old_pool = pool->clone();
  printf(".................a %d long pool built!\n", old_pool->getSize());
  new_pool = new CvGaborFeaturePool;
  init_weights();
  knn = new CvKNearest;
  svm = new CvSVM;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::loadweights(const char* filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::loadweights(const char* filename)
{
  cvReleaseMat( &weights );
  weights = (CvMat*)cvLoad( filename, NULL, NULL, NULL );
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::trainingsub(int job, int iter)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::trainingsub(int job, int iter)
{
  this->current = iter;
  char *featurefilename = new char[20];
  sprintf(featurefilename, "feature_%d_%d.txt", iter, job); 
  for (int i = 0; i < old_pool->getSize(); i++)
  {
    CvGaborFeature *feature;
    feature = old_pool->getfeature(i);
    double t_error = featureweak( feature );
    
    writefeature( featurefilename, feature, t_error, 0.0);
  }
  delete [] featurefilename;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::resumesub(int job, int iter)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::resumesub(int job, int iter)
{
  this->current = iter;
  char *featurefilename = new char[20];
  sprintf(featurefilename, "feature_%d_%d.txt", iter, job); 
  
  FILE * file;
  if ((file=fopen( featurefilename, "r" )) == NULL)
  {
    printf(featurefilename);
    exit(-1);
  }
  printf("Loading features from the log file %s", featurefilename);
  int x,y,Mu,Nu;
  float error;
  float alpha;
  int m = 0;
  int i;
  while (!feof(file))
  {
    if (fscanf(file, "%d ", &x) == EOF) break;
    if (fscanf(file, "%d ", &y) == EOF) break;
    if (fscanf(file, "%d ", &Nu) == EOF) break;
    if (fscanf(file, "%d ", &Mu) == EOF) break;
    if (fscanf(file, "%f ", &error) == EOF) break;
    if (fscanf(file, "%f\n", &alpha) == EOF) break;
    CvGaborFeature *feature = old_pool->getfeature( m );
    if((feature->getx()==x)&&(feature->gety()==y)&&(feature->getMu()==Mu)&&(feature->getNu()==Nu))
    {
      feature->seterror( error );
      m++;
    }
    else break;
  }
  printf("\n %d features read   ... ... done!\n", m);
  fclose(file);
  
  for (int i = m; i < old_pool->getSize(); i++)
  {
    CvGaborFeature *feature;
    feature = old_pool->getfeature(i);
    double t_error = featureweak( feature );
    
    writefeature( featurefilename, feature, t_error, 0.0);
  }
  
  
  delete [] featurefilename;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::findmin()
 */
double MultiAdaGabor::CvMultiGabAdaFSM1::findmin()
{
  double merror;
  if (old_pool->getSize() < 1)
  {
    return -1.0;
  }
  else
  { 
    CvGaborFeature *sfeature;
    do
    {
      sfeature = old_pool->min();
    }
    while(new_pool->isIn( sfeature ));
    
    merror = sfeature->geterror();
    signfeature(sfeature);
    current = new_pool->getSize() - 1;
    return merror;
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::loadsign(const char* filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::loadsign(const char* filename)
{
  delete new_pool;

  new_pool = new CvGaborFeaturePool;
  new_pool->load( filename );
  nselecfeatures = new_pool->getSize();
  
  // load alpha
  FILE * file;
  if ((file=fopen(filename,"r")) == NULL)
  {
    perror(filename);
    exit(1);
  }
  int x,y,Mu,Nu;
  float merror,alpha;
  while (!feof(file))
  {
    if (fscanf(file, " %d", &x) == EOF) break;
    if (fscanf(file, " %d", &y) == EOF) break;
    if (fscanf(file, " %d", &Nu) == EOF) break;
    if (fscanf(file, " %d", &Mu) == EOF) break;
    if (fscanf(file, " %f", &merror) == EOF) break;
    if (fscanf(file, " %f\n", &alpha) == EOF) break;
    alphas.push_back(alpha);
  }
  fclose(file);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::update()
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::update()
{
  if (old_pool->getSize() >= 1)
  {
    double error, alpha;
    CvGaborFeature *feature;
    feature = new_pool->getfeature(current);
    
    CvTrainingData *data;
    
    data = feature->_XM2VTSMulti_F((CvXm2vts*)database);
    data->setweights(weights);
    
    CvMWeakLearner *mweak = new CvMWeakLearner( (CvXm2vts*)database, CvWeakLearner::POTSU );
    mweak->train(data);
    
    for(int i = 0; i < nsamples; i++)
    {
      double weight = data->getweightofsample( i );
      cvSetReal1D(weights, i, weight);
    }
    
    error = mweak->get_error();
    alpha = log((1-error)/error)/2;
    writefeature( "signficant.txt", feature, error, alpha );
    
    printf("Push a multi-class weaklearner .......\n");
    mweaks.push_back(*mweak);
    alphas.push_back(alpha);
    normalise( weights );
    delete data;
  }
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::getNumFeatures()
 */
int  MultiAdaGabor::CvMultiGabAdaFSM1::getNumFeatures()
{
  return new_pool->getSize();
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::setCurrentIter(int c)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::setCurrentIter(int c)
{
  current = c;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::getCurrentIter()
 */
int MultiAdaGabor::CvMultiGabAdaFSM1::getCurrentIter()
{
  return current;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::predict(IplImage *img, int nweaks)
 */
CvMat* MultiAdaGabor::CvMultiGabAdaFSM1::predict(IplImage *img, int nweaks)
{
  if( nweaks > mweaks.size() )
  {
    perror("Number of weak learners exceeds the maximal!\n");
    exit(-1);
  }
  double vfeature, label, criterion = 0.0, flabel=0.0, alpha;
  CvMat *rank = cvCreateMat(1, nClass, CV_32FC1);
  cvSetZero( rank );
  for (int i = 0; i < nweaks; i++)
  {
      /* load feature value */
    
    CvGaborFeature *feature;
    feature = new_pool->getfeature(i);
    vfeature = feature->val( img, feature->getNu());
    
      /* get label from weak learner*/
    CvMat *resultMat = mweaks[i].mpredict( vfeature );

    CvMat *tmpMat = cvCreateMat( 1, nClass, CV_32FC1 ); 
    cvSetZero( tmpMat );
    CvSize size = cvGetSize( resultMat );
    int nCls = size.width;
    
    alpha = alphas[i];
    for(int j = 0; j < nCls; j++)
    {
      double c = cvGetReal1D( resultMat, j );
      cvSetReal1D( tmpMat, (int)(c-1), alpha );
    }
    
    //#ifdef DEBUG
    char *resultsname = new char[30];
    sprintf(resultsname, "results_%d.txt", i);
    //cvSave(resultsname, tmpMat, NULL, NULL, cvAttrList());
    FILE *fs;
    fs = fopen(resultsname, "w");
    for(int i = 0; i < nClass; i++)
    {
      double v = cvGetReal1D( tmpMat, i );
      fprintf(fs, "%f ", v);
    }
    fclose(fs);
    delete [] resultsname;
    //#endif
    
    //cvConvertScale( tmpMat, tmpMat, alpha, 0.0 );
    
    //cvAdd( rank, tmpMat, rank, NULL );
    for(int j = 0; j < nClass; j++)
    {
      double v1 = cvGetReal1D(tmpMat, j);
      double v2 = cvGetReal1D(rank, j);
      cvSetReal1D(rank, j , v1+v2);
    }

    cvReleaseMat( &resultMat );
    cvReleaseMat( &tmpMat );
  }
  
  
  double min_val, max_val;
  CvPoint min_loc, max_loc;
  cvMinMaxLoc( rank, &min_val, &max_val, &min_loc, &max_loc, NULL );

  /*  CvMat *cls = cvCreateMat(1, nClass, CV_32FC1);
  for(int i = 0; i < nClass; i++)
  {
    if(cvGetReal1D(rank, i) == max_val) cvSetReal1D(cls, i, 1.0);
    else cvSetReal1D(cls, i, 0.0);
  }

  cvReleaseMat( &rank );
  return cls;*/
  return rank;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::predict(const char *filename, int nweaks)
 */
CvMat* MultiAdaGabor::CvMultiGabAdaFSM1::predict(const char *filename, int nweaks)
{
  IplImage *img = cvLoadImage( filename, CV_LOAD_IMAGE_GRAYSCALE);
  CvMat *cls = predict(img, nweaks);
  return cls;
}



/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::predict(IplImage *img)
 */
CvMat* MultiAdaGabor::CvMultiGabAdaFSM1::predict(IplImage *img)
{
  return predict( img, mweaks.size());
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::predict(const char *filename)
 */
CvMat* MultiAdaGabor::CvMultiGabAdaFSM1::predict(const char *filename)
{
  return predict( filename, mweaks.size());
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::testing(const char* filelist, const char* resultfile, int nofeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::testing(const char* filelist, const char* resultfile, int nofeatures)
{
  printf("Start testing from %s ... ... \n", filelist);
  char *imgname = new char[50];
  FILE *fs = fopen(filelist, "r");
  assert(fs);
  FILE *file = fopen(resultfile,"w");
  assert(file);

  while(feof(fs) == 0)
  {
    if (fscanf(fs, "%s", imgname) == EOF) break;
    CvMat *cls = predict( imgname, nofeatures );
    
    char *cindexname = new char[50];
    sprintf( cindexname, "%s.xml", imgname );
    cvSave( cindexname, cls, NULL, NULL, cvAttrList() );
    delete [] cindexname;
    
    
    
    //sort the matrix
    
    
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* seq = cvCreateSeq( CV_32FC2, sizeof(CvSeq), sizeof(CvPoint2D32f), storage );
    
    for( int i = 0; i < nClass; i++ )
    {
      CvPoint2D32f pt;
      pt.x = (float)(i+1);
      pt.y = cvGetReal1D(cls, i);
      cvSeqPush( seq, &pt );
    }
    
    cvSeqSort( seq, cmp_func, 0 );
    

    fprintf( file, "%s ", imgname);
    printf( "%s is Class ", imgname);
    
    //get class no
    string str(imgname);
    size_t found;
    found = str.find_last_of("/\\");
    string str1 = str.substr(found+1);
    found = str1.find_last_of("_");
    string str2 = str1.substr(0, found);
    
    
    int sub = atoi(str2.c_str());
    
    for( int i = 0; i < seq->total; i++ )
    {
      CvPoint2D32f* pt = (CvPoint2D32f*)cvGetSeqElem( seq, i );
      if((int)(pt->x)==sub)
      {
        fprintf( file, " (%d %f) ", i, pt->y);
        printf( " (%d %f) ", i, pt->y);
        break;
      }
    }
    
    for(int i = 0; i < 10; i++)
    {
      CvPoint2D32f* pt = (CvPoint2D32f*)cvGetSeqElem( seq, i );
      fprintf( file, " %d %f ", (int)(pt->x), pt->y);
      printf( " %d %f ", (int)(pt->x), pt->y);
      
    }
    cvReleaseMat(&cls);
    
    
    
    fprintf( file, "\n ");
    printf( "\n");
    cvReleaseMemStorage( &storage );
  }
  delete [] imgname;
  fclose(file);
  fclose(fs);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::testing(const char* filelist, const char* resultfile)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::testing(const char* filelist, const char* resultfile)
{
  testing(filelist, resultfile, mweaks.size());
}



static int MultiAdaGabor::cmp_func( const void* _a, const void* _b, void* userdata )
{
  CvPoint2D32f* a = (CvPoint2D32f*)_a;
  CvPoint2D32f* b = (CvPoint2D32f*)_b;
  if (a->y > b->y) return -1;
  else if(a->y < b->y) return 1;
  else return 0;
}



/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::saveweaks(const char *filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::saveweaks(const char *filename)
{
  CvMemStorage* storage = cvCreateMemStorage(0);
  CvSeq* seq = cvCreateSeq(0, sizeof(CvSeq), sizeof(CvMGaborTree), storage );
  int num = mweaks.size();
  for(int i = 0; i < num; i++ )
  {
    CvGaborFeature *feature;
    feature = new_pool->getfeature(i);
    
    CvMGaborTree mtree;
    
    mtree.x = feature->getx();
    mtree.y = feature->gety();
    mtree.Mu = feature->getMu();
    mtree.Nu = feature->getNu();
    mtree.alpha = alphas[i];
    mtree.num = nClass;
    CvWeak* weak = new CvWeak[nClass];
    for(int j = 0; j < nClass; j++)
    {
      weak[j].threshold = mweaks[i].getTheshold(j);
      weak[j].parity = mweaks[i].getParity(j);
    }
    mtree.weak = &weak;
    cvSeqPush( seq, &mtree );
  }
  
  char *weakname = new char[20];
  char *classifiername = new char[20];
  CvMemStorage* xstorage = cvCreateMemStorage( 0 );
  CvFileStorage *fs = cvOpenFileStorage( filename, xstorage, CV_STORAGE_WRITE );
  for (int i = 0; i <seq->total; i++)
  {
    CvMGaborTree *atree = (CvMGaborTree*)cvGetSeqElem(seq, i);
    
    sprintf(weakname, "weak_%d", i);
    cvStartWriteStruct( fs, weakname, CV_NODE_MAP, "CvGaborTree", cvAttrList(0,0) );
    cvWriteInt(fs, "x", atree->x);
    cvWriteInt(fs, "y", atree->y);
    cvWriteInt(fs, "Mu", atree->Mu);
    cvWriteInt(fs, "Nu", atree->Nu);
    cvWriteInt(fs, "num", atree->num);
    cvWriteReal(fs, "alpha", atree->alpha);
    int nC = atree->num;

    for(int j = 0; j < nC; j++)
    {
      sprintf(classifiername, "classifier_%d", j);
      cvStartWriteStruct( fs, classifiername, CV_NODE_MAP, "CvWeak", cvAttrList(0,0) );
      double t = mweaks[i].getTheshold(j);
      double p = mweaks[i].getParity(j);
      cvWriteReal(fs, "threshold", t);
      cvWriteReal(fs, "parity", p);
      cvEndWriteStruct( fs );
    }

    cvEndWriteStruct( fs ); 
  } 
  
    /* release memory storage in the end */
  delete [] weakname;
  delete [] classifiername;
  cvReleaseMemStorage( &xstorage );
  cvReleaseMemStorage( &storage );
  cvReleaseFileStorage(&fs);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::loadweaks(const char* filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::loadweaks(const char* filename)
{
  //clear();
  {
    mweaks.clear();
    delete new_pool;
  }
  CvMemStorage* fstorage = cvCreateMemStorage( 0 );
  CvFileStorage *fs;
  fs = cvOpenFileStorage( filename, fstorage, CV_STORAGE_READ );
  CvFileNode *root = cvGetRootFileNode( fs, 0);
  char *weakname = new char[20];
  int i = 0;
  
  CvMemStorage* storage = cvCreateMemStorage(0);
  
  while(1)
  {
    sprintf( weakname, "weak_%d", i);
    CvFileNode *weaknode = cvGetFileNodeByName( fs, root, weakname);
    if (!weaknode) break;
    CvMWeakLearner *mweak = new CvMWeakLearner;
    mweaknode2mweak(weaknode, fs, mweak);
    mweaks.push_back( *mweak );
    i++;	
  }
  

  cvReleaseFileStorage(&fs);
  
  delete [] weakname;
  
    /* set member variables */
  current = new_pool->getSize();
  nexpfeatures = new_pool->getSize();
  nselecfeatures = new_pool->getSize();
  printf(" %d weak classifiers have been loaded!\n", nselecfeatures);
  //cvReleaseMemStorage( &fstorage );
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::mweaknode2tree(CvFileNode *node, CvFileStorage *fs, CvMGaborTree *mtree);
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::mweaknode2tree(CvFileNode *node, CvFileStorage *fs, CvMGaborTree *mtree)
{
  CvFileNode *xnode = cvGetFileNodeByName( fs, node, "x");
  CvFileNode *ynode = cvGetFileNodeByName( fs, node, "y");
  CvFileNode *Munode = cvGetFileNodeByName( fs, node, "Mu");
  CvFileNode *Nunode = cvGetFileNodeByName( fs, node, "Nu");
  CvFileNode *numnode = cvGetFileNodeByName( fs, node, "num");
  CvFileNode *anode = cvGetFileNodeByName( fs, node, "alpha");
  int x = cvReadInt( xnode, 0 );
  int y = cvReadInt( ynode, 0 );
  int Mu = cvReadInt( Munode, 0 );
  int Nu = cvReadInt( Nunode, 0 );
  int num = cvReadInt( numnode, 0 );
  double alpha = cvReadReal( anode, 0);

  CvWeak* weak = new CvWeak[num];
  for(int i = 0; i < num; i++)
  {
    char *classifiername = new char[30];
    sprintf(classifiername, "classifier_%d", i);
    CvFileNode *weaknode = cvGetFileNodeByName( fs, node, classifiername );
    node2CvWeak( weaknode, fs, &weak[i] );
    delete [] classifiername;
  }
  mtree->x = x;
  mtree->y = y;
  mtree->Mu = Mu;
  mtree->Nu = Nu;
  mtree->alpha = alpha;
  mtree->num = num;
  mtree->weak = &weak;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::node2CvWeak(CvFileNode *node, CvFileStorage *fs, CvWeak *weak)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::node2CvWeak(CvFileNode *node, CvFileStorage *fs, CvWeak *weak)
{
  CvFileNode *tnode = cvGetFileNodeByName( fs, node, "threshold");
  CvFileNode *pnode = cvGetFileNodeByName( fs, node, "parity");
  double threshold = cvReadReal( tnode, 0);
  double parity = cvReadReal( pnode, 0);
  weak->parity = parity;
  weak->threshold = threshold;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::mweaknode2mweak(CvFileNode *node, CvFileStorage *fs, CvMWeakLearner *mweak)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::mweaknode2mweak(CvFileNode *node, CvFileStorage *fs, CvMWeakLearner *mweak)
{
  CvFileNode *xnode = cvGetFileNodeByName( fs, node, "x");
  CvFileNode *ynode = cvGetFileNodeByName( fs, node, "y");
  CvFileNode *Munode = cvGetFileNodeByName( fs, node, "Mu");
  CvFileNode *Nunode = cvGetFileNodeByName( fs, node, "Nu");
  CvFileNode *numnode = cvGetFileNodeByName( fs, node, "num");
  CvFileNode *anode = cvGetFileNodeByName( fs, node, "alpha");
  int x = cvReadInt( xnode, 0 );
  int y = cvReadInt( ynode, 0 );
  int Mu = cvReadInt( Munode, 0 );
  int Nu = cvReadInt( Nunode, 0 );
  int num = cvReadInt( numnode, 0 );
  double alpha = cvReadReal( anode, 0);
  
  mweak->init(num, 0);
  for(int i = 0; i < num; i++)
  {
    char *classifiername = new char[30];
    sprintf(classifiername, "classifier_%d", i);
    CvFileNode *weaknode = cvGetFileNodeByName( fs, node, classifiername );
    CvWeakLearner *weak = mweak->getWeakLearner( i );
    node2weak( weaknode, fs, weak );
    delete [] classifiername;
  }
  
  CvGaborFeature *feature = new CvGaborFeature( x, y, Mu, Nu);
  new_pool->add( feature );
  alphas.push_back( alpha );
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::node2weak(CvFileNode *node, CvFileStorage *fs, CvWeakLearner *weak)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::node2weak(CvFileNode *node, CvFileStorage *fs, CvWeakLearner *weak)
{
  CvFileNode *tnode = cvGetFileNodeByName( fs, node, "threshold");
  CvFileNode *pnode = cvGetFileNodeByName( fs, node, "parity");
  double threshold = cvReadReal( tnode, 0);
  double parity = cvReadReal( pnode, 0);
  weak->setType(CvWeakLearner::POTSU);
  weak->setparity( parity );
  weak->setthreshold( threshold );
}


/*!
\fn MultiAdaGabor::CvMultiGabAdaFSM1::knnpredict(IplImage *img, int nfeatures)
 */
CvMat* MultiAdaGabor::CvMultiGabAdaFSM1::knnpredict(IplImage *img, int nfeatures)
{
  if( nfeatures > new_pool->getSize())
  {
    perror("Number of features exceeds the maximal!\n");
    exit(-1);
  }

  CvMat *test_sample = cvCreateMat(1, nfeatures, CV_32FC1);
  double vfeature;
  for (int i = 0; i < nfeatures; i++)
  {
      /* load feature value */
    CvGaborFeature *feature;
    feature = new_pool->getfeature( i );
    vfeature = feature->val( img, feature->getNu() );
    cvSetReal1D( test_sample, i, vfeature );
  }
  
  float *neighbors = new float[max_k];
  CvMat *neighbor_responses = cvCreateMat(1, max_k, CV_32FC1);
  CvMat *dist = cvCreateMat(1, max_k, CV_32FC1);
  float c = knn->find_nearest( test_sample, max_k, 0, 0, neighbor_responses, dist );
  //printf("The class number is %f\n", c);
 
  
  
  cvReleaseMat( &dist );
  cvReleaseMat( &test_sample ); 
  //cvReleaseMat( &neighbor_responses );
  delete neighbors;
  //CvMat *result = cvCreateMat(1, max_k, CV_32FC1);
  //cvSetReal1D(result, 0, c);
  return neighbor_responses;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knntraining(int nfeatures, int k)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::knntraining(int nfeatures, int k)
{
  if( nfeatures > new_pool->getSize())
  {
    perror("Number of features exceeds the maximal!\n");
    exit(-1);
  }
  
  max_k = k;
  CvXm2vts *xm2vts = (CvXm2vts*)database;
  
  int nTrainingExample = xm2vts->get_num_sub()*xm2vts->get_num_pic();
  CvMat* trainData = cvCreateMat(nTrainingExample, nfeatures, CV_32FC1);
  
  CvMat* response = cvCreateMat(nTrainingExample, 1, CV_32FC1);

  for (int i = 0; i < nfeatures; i++)
  {
      /* load feature value */
    
    CvGaborFeature *feature;
    feature = new_pool->getfeature(i);
    
    CvTrainingData *data = feature->_XM2VTSMulti_F( (CvXm2vts*)database );
    int nexamples = data->getnumsample();
    CvMat *datamat = data->getdata();
    if( i == 0)
    {
      CvMat *dataresponse = data->getresponse();
      cvCopy(dataresponse, response);
      cvReleaseMat(&dataresponse);
    }
    for (int j = 0; j < nexamples; j++)
    {
      double vfeature = cvGetReal1D(datamat, j);
      cvSetReal2D(trainData, j, i, vfeature);
    }
    //vfeature = feature->val( img );
    cvReleaseMat(&datamat);
    delete data;
  }
  
  //CvSize size = cvGetSize(trainData);
  knn->train( trainData, response, 0, false, max_k, false );
  
  
  cvReleaseMat(&response);
  cvReleaseMat(&trainData);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knntesting(const char* filelist, const char* resultfile, int nofeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::knntesting(const char* filelist, const char* resultfile, int nofeatures)
{
  printf("Start testing from %s ... ... \n", filelist);
  char *imgname = new char[50];
  FILE *fs = fopen(filelist, "r");
  assert(fs);
  FILE *file = fopen(resultfile,"w");
  assert(file);
  
  while(feof(fs) == 0)
  {
    if (fscanf(fs, "%s", imgname) == EOF) break;
    printf("%s      ", imgname);
    IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
    knnpredict( img, nofeatures );
    cvReleaseImage( &img );
  }
  
  delete [] imgname;
  fclose(file);
  fclose(fs);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knnsave(const char* filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::knnsave(const char* filename)
{
  knn->save( filename, 0 );
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knnload(const char* filename)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::knnload(const char* filename)
{
  knn->clear();
  knn->load( filename, 0 );
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knntesting(const char* resultfile, int nofeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::knntesting(const char* resultfile, int nofeatures)
{
  const char * pathname = "/home/sir/sir02mz/local/FaceDB/XM2VTS/";
  FILE *file = fopen(resultfile,"w");
  assert(file);
  
  // testing the training examples
  int sub, pic;
  char *imgname = new char[50];
  int accept_training = 0, accept_testing = 0;;
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 1; pic <= 4; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      int c = (int)knnpredict1( img, nofeatures );
      fprintf( file, "%s            %d   \n",imgname, c);
      if( c == sub) accept_training++;
      cvReleaseImage( &img );
    }
  }
  
  fprintf( file, "\n\n\n");
  // testing the testing examples
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 5; pic <= 8; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      int c = (int)knnpredict1( img, nofeatures );
      fprintf( file, "%s            %d   \n",imgname, c);
      if( c == sub) accept_testing++;
      cvReleaseImage( &img );
    }
  }
  
  int impostor_false = 0;
  /* for ( sub = 201; sub <= 295; sub++)
  {
    for (pic = 1; pic <= 8; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      CvMat * result = knnpredict( img, nofeatures );
      int c = (int)cvGetReal1D(result, 0);
      fprintf( file, "%s            %d   \n",imgname, c);
      if( c == sub) impostor_false++;
      cvReleaseImage( &img );
      cvReleaseMat(&result);
    }
  }
  */
  double training_error = (double)(800-accept_training) /800.0;
  double testing_error = (double)(800-accept_testing)/800.0;
  fprintf( file, "%d     %d      %d\n", accept_training, accept_testing, impostor_false);
  fprintf( file, "%f         %f\n", training_error, testing_error );
  delete [] imgname;
  fclose( file );
  knn->clear();
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::svmtraining(int nfeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::svmtraining(int nfeatures)
{
  if( nfeatures > new_pool->getSize())
  {
    perror("Number of features exceeds the maximal!\n");
    exit(-1);
  }
  
 
  CvXm2vts *xm2vts = (CvXm2vts*)database;
  
  int nTrainingExample = xm2vts->get_num_sub()*xm2vts->get_num_pic();
  CvMat* trainData = cvCreateMat(nTrainingExample, nfeatures, CV_32FC1);
  
  CvMat* response = cvCreateMat(nTrainingExample, 1, CV_32FC1);
  printf("Training an SVM classifier  ................\n");
  for (int i = 0; i < nfeatures; i++)
  {
      /* load feature value */
    
    CvGaborFeature *feature;
    feature = new_pool->getfeature(i);
    
    CvTrainingData *data = feature->_XM2VTSMulti_F( (CvXm2vts*)database );
    int nexamples = data->getnumsample();
    CvMat *datamat = data->getdata();
    if( i == 0)
    {
      CvMat *dataresponse = data->getresponse();
      cvCopy(dataresponse, response);
      cvReleaseMat(&dataresponse);
    }
    for (int j = 0; j < nexamples; j++)
    {
      double vfeature = cvGetReal1D(datamat, j);
      cvSetReal2D(trainData, j, i, vfeature);
    }
    cvReleaseMat(&datamat);
    delete data;
  }
  
  CvTermCriteria term_crit = cvTermCriteria( CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 200, 0.8);
  
  /*Type of SVM, one of the following types:
    CvSVM::C_SVC - n-class classification (n>=2), allows imperfect separation of classes with penalty multiplier C for outliers.
    CvSVM::NU_SVC - n-class classification with possible imperfect separation. Parameter nu (in the range 0..1, the larger the value, the smoother the decision boundary) is used instead of C.
    CvSVM::ONE_CLASS - one-class SVM. All the training data are from the same class, SVM builds a boundary that separates the class from the rest of the feature space.
    CvSVM::EPS_SVR - regression. The distance between feature vectors from the training set and the fitting hyperplane must be less than p. For outliers the penalty multiplier C is used.
    CvSVM::NU_SVR - regression; nu is used instead of p. */
  int _svm_type = CvSVM::NU_SVC;
  
  /*The kernel type, one of the following types:
    CvSVM::LINEAR - no mapping is done, linear discrimination (or regression) is done in the original feature space. It is the fastest option. d(x,y) = x•y == (x,y)
    CvSVM::POLY - polynomial kernel: d(x,y) = (gamma*(x•y)+coef0)degree
    CvSVM::RBF - radial-basis-function kernel; a good choice in most cases: d(x,y) = exp(-gamma*|x-y|2)
    CvSVM::SIGMOID - sigmoid function is used as a kernel: d(x,y) = tanh(gamma*(x•y)+coef0) */
  
  int _kernel_type = CvSVM::POLY;
  
  double _degree = 3.0;
  double _gamma = 1.0;
  double _coef0 = 0.0;
  double _C = 1.0;
  double _nu = 1.0;
  double _p = 1.0;
  
  CvSVMParams  params( CvSVM::C_SVC, CvSVM::POLY, _degree, _gamma, _coef0, _C, _nu, _p,
                                        0, term_crit );
  
  svm->train( trainData, response, 0, 0, params );
  cvReleaseMat(&response);
  cvReleaseMat(&trainData);
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::svmtesting(const char* resultfile, int nofeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::svmtesting(const char* resultfile, int nofeatures)
{
  const char * pathname = "/home/sir/sir02mz/local/FaceDB/XM2VTS/";
  FILE *file = fopen(resultfile,"w");
  assert(file);
  
  // testing the training examples
  int sub, pic;
  char *imgname = new char[50];
  int accept_training = 0, accept_testing = 0;;
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 1; pic <= 4; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      float result = svmpredict( img, nofeatures );
      int c = (int)result;
      fprintf( file, "%s            %d   \n",imgname, c);
      if( c == sub) accept_training++;
      cvReleaseImage( &img );

    }
  }
  
  fprintf( file, "\n\n\n");
  // testing the testing examples
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 5; pic <= 8; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      float result = svmpredict( img, nofeatures );
      int c = (int)result;
      if( c == sub) accept_testing++;
      fprintf( file, "%s            %d   \n",imgname, c);
      cvReleaseImage( &img );

    }
  }
  
  int impostor_false = 0;
  /* for ( sub = 201; sub <= 295; sub++)
  {
    for (pic = 1; pic <= 8; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      CvMat * result = knnpredict( img, nofeatures );
      int c = (int)cvGetReal1D(result, 0);
      fprintf( file, "%s            %d   \n",imgname, c);
      if( c == sub) impostor_false++;
      cvReleaseImage( &img );
      cvReleaseMat(&result);
    }
  }
  */
  double training_error = (double)(800-accept_training) /800.0;
  double testing_error = (double)(800-accept_testing)/800.0;
  fprintf( file, "%d     %d      %d\n", accept_training, accept_testing, impostor_false);
  fprintf( file, "%f         %f\n", training_error, testing_error );
  delete [] imgname;
  fclose( file );
  svm->clear();
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::svmpredict(IplImage *img, int nfeatures)
 */
float MultiAdaGabor::CvMultiGabAdaFSM1::svmpredict(IplImage *img, int nfeatures)
{
  if( nfeatures > new_pool->getSize())
  {
    perror("Number of features exceeds the maximal!\n");
    exit(-1);
  }
  
  CvMat *test_sample = cvCreateMat(1, nfeatures, CV_32FC1);
  double vfeature;
  for (int i = 0; i < nfeatures; i++)
  {
      /* load feature value */
    CvGaborFeature *feature;
    feature = new_pool->getfeature( i );
    vfeature = feature->val( img, feature->getNu() );
    cvSetReal1D( test_sample, i, vfeature );
  }
  
  
  float result = svm->predict( test_sample );
  printf("The class number is %f\n", result);

  return result;
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::lossknntesting(const char* resultfile, int nofeatures)
 */
void MultiAdaGabor::CvMultiGabAdaFSM1::lossknntesting(const char* resultfile, int nofeatures)
{
  const char * pathname = "/home/sir/sir02mz/local/FaceDB/XM2VTS/";
  FILE *file = fopen(resultfile,"w");
  assert(file);
  
  // testing the training examples
  int sub, pic;
  char *imgname = new char[50];
  int accept_training = 0, accept_testing = 0;
  int c;
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 1; pic <= 4; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      CvMat * result = knnpredict( img, nofeatures );
      
      CvMat *cls = cvCreateMat( 1, 32, CV_32FC1 );
      int index = 0;
      for (int i = 0; i < max_k; i++)
      {
        double val = cvGetReal1D( result, i );
        if (i != 0)
        {
          if ( ! isInMat( cls, val ) )
          {
            cvSetReal1D( cls, index, val);
            index++;
          }
        }
        else
        {
          cvSetReal1D( cls, index, val);
          index++;
        }
      }
      cvReleaseMat( &cls );
      
      
      bool correct = false;
      for (int i = 0; i < max_k; i++)
      {
        c = (int)cvGetReal1D(result, i);
        if( c == sub)
        {
          correct = true;
          break;
        }
      }
      if ( correct ) 
      {
        printf("       The class number is %d  %d\n", c, index);
        fprintf( file, "%s            %d   %d\n",imgname, c, index);
        accept_training++;
      }
      else
      {
        printf( "       misclassified.\n  %d", index );
        fprintf( file, "%s            %d   %d\n",imgname, 0, index);
      }
    
      cvReleaseImage( &img );
      cvReleaseMat(&result);
    }
  }
  
  fprintf( file, "\n\n\n");
  // testing the testing examples
  for ( sub = 1; sub <= 200; sub++)
  {
    for (pic = 5; pic <= 8; pic++)
    {
      sprintf( imgname, "%s/%d_%d.bmp", pathname, sub, pic);
      printf("%s      ", imgname);
      IplImage *img = cvLoadImage( imgname, CV_LOAD_IMAGE_GRAYSCALE);
      CvMat * result = knnpredict( img, nofeatures );
      
      CvMat *cls = cvCreateMat( 1, 32, CV_32FC1 );
      int index = 0;
      for (int i = 0; i < max_k; i++)
      {
        double val = cvGetReal1D( result, i );
        if (i != 0)
        {
          if ( ! isInMat( cls, val ) )
          {
            cvSetReal1D( cls, index, val);
            index++;
          }
        }
        else
        {
          cvSetReal1D( cls, index, val);
          index++;
        }
      }
      cvReleaseMat( &cls );
      
      bool correct = false;
      for (int i = 0; i < max_k; i++)
      {
        c = (int)cvGetReal1D(result, i);
        if( c == sub)
        {
          correct = true;
          break;
        }
      }
      if ( correct ) 
      {
        printf("       The class number is %d      %d\n", c, index);
        fprintf( file, "%s            %d   %d\n",imgname, c, index);
        accept_testing++;
      }
      else
      {
        printf( "       misclassified.    %d\n", index );
        fprintf( file, "%s            %d     %d\n",imgname, 0, index);
      }

      cvReleaseImage( &img );
      cvReleaseMat(&result);
    }
  }
  
  int impostor_false = 0;

  double training_error = (double)(800-accept_training) /800.0;
  double testing_error = (double)(800-accept_testing)/800.0;
  fprintf( file, "%d     %d      %d\n", accept_training, accept_testing, impostor_false);
  fprintf( file, "%f         %f\n", training_error, testing_error );
  delete [] imgname;
  fclose( file );
  knn->clear();
}


/*!
    \fn MultiAdaGabor::CvMultiGabAdaFSM1::knnpredict1(IplImage *img, int nfeatures)
 */
float MultiAdaGabor::CvMultiGabAdaFSM1::knnpredict1(IplImage *img, int nfeatures)
{
  if( nfeatures > new_pool->getSize())
  {
    perror("Number of features exceeds the maximal!\n");
    exit(-1);
  }
  
  CvMat *test_sample = cvCreateMat(1, nfeatures, CV_32FC1);
  double vfeature;
  for (int i = 0; i < nfeatures; i++)
  {
      /* load feature value */
    CvGaborFeature *feature;
    feature = new_pool->getfeature( i );
    vfeature = feature->val( img, feature->getNu() );
    cvSetReal1D( test_sample, i, vfeature );
  }
  
  float *neighbors = new float[max_k];
  CvMat *neighbor_responses = cvCreateMat(1, max_k, CV_32FC1);
  CvMat *dist = cvCreateMat(1, max_k, CV_32FC1);
  float c = knn->find_nearest( test_sample, max_k, 0, 0, neighbor_responses, dist );
  printf("The class number is %f\n", c);
  
  
  
  cvReleaseMat( &dist );
  cvReleaseMat( &test_sample ); 
  cvReleaseMat( &neighbor_responses );
  delete neighbors;
  
  return c;
}
