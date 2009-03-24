#include <iostream>
#include <string>
using namespace std;

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Linpack.h>
#include <R_ext/Lapack.h>
#include <R_ext/BLAS.h>
#include "util.h"
#include "covmodel.h"



extern "C" {

  SEXP spPPMvGLM(SEXP Y_r, SEXP X_r, SEXP p_r, SEXP n_r, SEXP m_r, SEXP coordsD_r,SEXP family_r,
		 SEXP isModPp_r, SEXP q_r, SEXP knotsD_r, SEXP coordsKnotsD_r,
		 SEXP betaPrior_r, SEXP betaNorm_r, SEXP KIW_r, SEXP nuUnif_r, SEXP phiUnif_r,
		 SEXP phiStarting_r, SEXP AStarting_r, SEXP nuStarting_r, SEXP betaStarting_r, SEXP w_strStarting_r,
		 SEXP phiTuning_r, SEXP ATuning_r, SEXP nuTuning_r , SEXP betaTuning_r, SEXP w_strTuning_r,
		 SEXP covModel_r, SEXP nSamples_r, SEXP verbose_r, SEXP nReport_r){


    /*****************************************
                Common variables
    *****************************************/
    int i,j,k,l,info,nProtect= 0;
    char const *lower = "L";
    char const *upper = "U";
    char const *ntran = "N";
    char const *ytran = "T";
    char const *rside = "R";
    char const *lside = "L";
    const double one = 1.0;
    const double negOne = -1.0;
    const double zero = 0.0;
    const int incOne = 1;

    /*****************************************
                     Set-up
    *****************************************/

    double *Y = REAL(Y_r);
    double *X = REAL(X_r);
    int p = INTEGER(p_r)[0];
    int pp = p*p;
    int n = INTEGER(n_r)[0];
    int m = INTEGER(m_r)[0];
    int nLTr = m*(m-1)/2+m;

    string family = CHAR(STRING_ELT(family_r,0));

    //covariance model
    string covModel = CHAR(STRING_ELT(covModel_r,0));

    //if predictive process
    bool isModPp = static_cast<bool>(INTEGER(isModPp_r)[0]);

    int q = INTEGER(q_r)[0];
    double *knotsD = REAL(knotsD_r);
    double *coordsKnotsD = REAL(coordsKnotsD_r);

    //priors and starting
    string betaPrior = CHAR(STRING_ELT(betaPrior_r,0));

    double *betaMu = NULL;
    double *betaSd = NULL;
    
    if(betaPrior == "normal"){
      betaMu = REAL(VECTOR_ELT(betaNorm_r, 0)); 
      betaSd = REAL(VECTOR_ELT(betaNorm_r, 1));
    }
    
    double *phiUnif = REAL(phiUnif_r);
    double KIW_df = REAL(VECTOR_ELT(KIW_r, 0))[0]; double *KIW_S = REAL(VECTOR_ELT(KIW_r, 1));

    double *phiStarting = REAL(phiStarting_r);
    double *AStarting = REAL(AStarting_r);
    double *betaStarting = REAL(betaStarting_r);
    double *w_strStarting = REAL(w_strStarting_r);

    //if matern
    double *nuUnif = NULL;
    double *nuStarting = NULL;

    if(covModel == "matern"){
      nuUnif = REAL(nuUnif_r);
      nuStarting = REAL(nuStarting_r);
    }

    //tuning
    double *betaTuning = (double *) R_alloc(p*p, sizeof(double)); 
    F77_NAME(dcopy)(&pp, REAL(betaTuning_r), &incOne, betaTuning, &incOne);
    double *phiTuning = REAL(phiTuning_r);
    double *ATuning = REAL(ATuning_r);
    double *w_strTuning = REAL(w_strTuning_r);
    double *nuTuning = NULL;

    if(covModel == "matern")
      nuTuning = REAL(nuTuning_r);

    int nSamples = INTEGER(nSamples_r)[0];
    int verbose = INTEGER(verbose_r)[0];
    int nReport = INTEGER(nReport_r)[0];


    if(verbose){
      Rprintf("----------------------------------------\n");
      Rprintf("\tGeneral model description\n");
      Rprintf("----------------------------------------\n");
      Rprintf("Model fit with %i observations.\n\n", n);
      Rprintf("Number of covariates %i (including intercept if specified).\n\n", p);
      Rprintf("Using the %s spatial correlation model.\n\n", covModel.c_str());
      
      if(isModPp)
	Rprintf("Using modified predictive process with %i knots.\n\n", q);
      else
	Rprintf("Using non-modified predictive process with %i knots.\n\n", q);
      
      Rprintf("Number of MCMC samples %i.\n\n", nSamples);

      Rprintf("Priors and hyperpriors:\n");
  
      if(betaPrior == "flat"){
	Rprintf("\tbeta flat.\n");
      }else{
	Rprintf("\tbeta normal:\n");
	Rprintf("\t\tmu:"); printVec(betaMu, p);
	Rprintf("\t\tsd:"); printVec(betaSd, p);Rprintf("\n");
      }
      Rprintf("\n");


      Rprintf("\tbeta flat.\n\n");   
      Rprintf("\tK IW hyperpriors df=%.5f\n", KIW_df);
      printMtrx(KIW_S, m, m);
      Rprintf("\n"); 

      Rprintf("\tphi Unif hyperpriors\n");
      Rprintf("\t");   
      for(i = 0; i < m; i++){
	Rprintf("(%.5f, %.5f) ", phiUnif[i*2], phiUnif[i*2+1]);
      }
      Rprintf("\n\n");   

      if(covModel == "matern"){
	Rprintf("\tnu Unif hyperpriors\n");
	Rprintf("\t");
	for(i = 0; i < m; i++){
	  Rprintf("(%.5f, %.5f) ", nuUnif[i*2], nuUnif[i*2+1]);
	}
	Rprintf("\n\n");   
      }

      Rprintf("Metropolis tuning values:\n");

      Rprintf("\tbeta tuning:\n");
      printMtrx(betaTuning, p, p);
      Rprintf("\n"); 
  
      Rprintf("\tA tuning:\n");
      Rprintf("\t"); printVec(ATuning, nLTr);
      Rprintf("\n"); 

      Rprintf("\tphi tuning\n");
      Rprintf("\t"); printVec(phiTuning, m);
      Rprintf("\n");   

      if(covModel == "matern"){
	Rprintf("\tnu tuning\n");
	Rprintf("\t"); printVec(nuTuning, m);
	Rprintf("\n");
      }

      Rprintf("Metropolis starting values:\n");
  
      Rprintf("\tbeta starting:\n");
      Rprintf("\t"); printVec(betaStarting, p);
      Rprintf("\n"); 

      Rprintf("\tA starting:\n");
      Rprintf("\t"); printVec(AStarting, nLTr);
      Rprintf("\n"); 

      Rprintf("\tphi starting\n");
      Rprintf("\t"); printVec(phiStarting, m);
      Rprintf("\n");   

      if(covModel == "matern"){
	Rprintf("\t"); Rprintf("\tnu starting\n");
	printVec(nuStarting, m);
      }
    }
 
    /*****************************************
        Set-up cov. model function pointer
    *****************************************/
    int nPramPtr = 1;
    
    void (covmodel::*cov1ParamPtr)(double, double &, double &) = NULL; 
    void (covmodel::*cov2ParamPtr)(double, double, double &, double&) = NULL;
    
    if(covModel == "exponential"){
      cov1ParamPtr = &covmodel::exponential;
    }else if(covModel == "spherical"){
      cov1ParamPtr = &covmodel::spherical;
    }else if(covModel == "gaussian"){
      cov1ParamPtr = &covmodel::gaussian;
    }else if(covModel == "matern"){
      cov2ParamPtr = &covmodel::matern;
      nPramPtr = 2;
    }else{
      error("c++ error: cov.model is not correctly specified");
    }
   
    //my covmodel object for calling cov function
    covmodel *covModelObj = new covmodel;

    /*****************************************
         Set-up MCMC sample matrices etc.
    *****************************************/
    int nn = n*n;
    int mm = m*m;
    int nm = n*m;
    int nq = n*q;
    int qq = q*q;
    int qm = q*m;
    int qmqm = qm*qm;

    //spatial parameters
    int nParams, betaIndx, AIndx, LIndx, phiIndx, nuIndx;

    if(covModel != "matern"){
      nParams = p+nLTr+m;//A, phi
      betaIndx = 0; AIndx = betaIndx+p; phiIndx = AIndx+nLTr;
    }else {
      nParams = p+nLTr+2*m;//A, phi, nu
      betaIndx = 0; AIndx = betaIndx+p; phiIndx = AIndx+nLTr, nuIndx = phiIndx+m;
    }
    
    double *spParams = (double *) R_alloc(nParams, sizeof(double));
    
    //set starting
    F77_NAME(dcopy)(&p, betaStarting, &incOne, &spParams[betaIndx], &incOne);

    covTrans(AStarting, &spParams[AIndx], m);

    for(i = 0; i < m; i++){
      spParams[phiIndx+i] = logit(phiStarting[i], phiUnif[i*2], phiUnif[i*2+1]);
    }

    if(covModel == "matern"){
      for(i = 0; i < m; i++){
	spParams[nuIndx+i] = logit(nuStarting[i], nuUnif[i*2], nuUnif[i*2+1]);
      }
    }

    double *wCurrent = (double *) R_alloc(nm, sizeof(double));
    double *w_strCurrent = (double *) R_alloc(qm, sizeof(double));
    F77_NAME(dcopy)(&qm, w_strStarting, &incOne, w_strCurrent, &incOne);
   
    //samples and random effects
    SEXP w_r, w_str_r, samples_r, accept_r;

    PROTECT(w_r = allocMatrix(REALSXP, nm, nSamples)); nProtect++; 
    double *w = REAL(w_r); zeros(w, nm*nSamples);

    PROTECT(w_str_r = allocMatrix(REALSXP, qm, nSamples)); nProtect++; 
    double *w_str = REAL(w_str_r); zeros(w_str, qm*nSamples);

    PROTECT(samples_r = allocMatrix(REALSXP, nParams, nSamples)); nProtect++; 
    double *samples = REAL(samples_r);

    PROTECT(accept_r = allocMatrix(REALSXP, 1, 1)); nProtect++;

    /*****************************************
       Set-up MCMC alg. vars. matrices etc.
    *****************************************/
    int s=0, status=0, rtnStatus=0, accept=0, batchAccept = 0;
    double logPostCurrent = 0, logPostCand = 0, detCand = 0, logDetK, SKtrace;

    double *ct = (double *) R_alloc(nm*qm, sizeof(double));
    double *C_str = (double *) R_alloc(qmqm, sizeof(double));

    double *tmp_mm = (double *) R_alloc(mm, sizeof(double));
    double *tmp_mm1 = (double *) R_alloc(mm, sizeof(double));
    double *tmp_nm = (double *) R_alloc(nm, sizeof(double));
    double *tmp_qm = (double *) R_alloc(qm, sizeof(double));
    double *tmp_nmqm = (double *) R_alloc(nm*qm, sizeof(double));

    double *candSpParams = (double *) R_alloc(nParams, sizeof(double));
    double *beta = (double *) R_alloc(p, sizeof(double));
    double *A = (double *) R_alloc(mm, sizeof(double));
 
    double *theta = (double *) R_alloc(mm, sizeof(double));
    double *w_strCand = (double *) R_alloc(qm, sizeof(double));
    double *wCand = (double *) R_alloc(nm, sizeof(double));

    double logMHRatio;


    if(verbose){
      Rprintf("-------------------------------------------------\n");
      Rprintf("\t\tSampling\n");
      Rprintf("-------------------------------------------------\n");
      #ifdef Win32
        R_FlushConsole();
      #endif
    }

    logPostCurrent = R_NegInf;

    GetRNGstate();
    for(s = 0; s < nSamples; s++){
 
      //propose   
      mvrnorm(&candSpParams[betaIndx], &spParams[betaIndx], betaTuning, p, false);
      F77_NAME(dcopy)(&p, &candSpParams[betaIndx], &incOne, beta, &incOne);

      for(i = 0; i < nLTr; i++){
	candSpParams[AIndx+i] = rnorm(spParams[AIndx+i], ATuning[i]);
      }

      covTransInvExpand(&candSpParams[AIndx], A, m);

      for(i = 0; i < m; i++){
	candSpParams[phiIndx+i] = rnorm(spParams[phiIndx+i], phiTuning[i]);
	theta[i] = logitInv(candSpParams[phiIndx+i], phiUnif[i*2], phiUnif[i*2+1]);
	
	if(covModel == "matern"){
	  candSpParams[nuIndx+i] = rnorm(spParams[nuIndx+i], nuTuning[i]);
	  theta[m+i] = logitInv(candSpParams[nuIndx+i], nuUnif[i*2], nuUnif[i*2+1]);
	}
      }

      for(i = 0; i < qm; i++){
	w_strCand[i] = rnorm(w_strCurrent[i], sqrt(w_strTuning[i]));
      }   
      
      //make ct
      for(i = 0; i < n; i++){
	for(j = 0; j < q; j++){
	  
	  zeros(tmp_mm, mm);
	  
	  for(k = 0; k < m; k++){
	    if(nPramPtr == 1)
	      (covModelObj->*cov1ParamPtr)(theta[k], tmp_mm[k*m+k], coordsKnotsD[j*n+i]);
	    else //i.e., 2 parameter matern
	      (covModelObj->*cov2ParamPtr)(theta[k], theta[m+k], tmp_mm[k*m+k], coordsKnotsD[j*n+i]);
	  }
	  
	  F77_NAME(dgemm)(ntran, ntran, &m, &m, &m, &one, A, &m, tmp_mm, &m, &zero, tmp_mm1, &m);
	  F77_NAME(dgemm)(ntran, ytran, &m, &m, &m, &one, tmp_mm1, &m, A, &m, &zero, tmp_mm, &m);
	  
	  for(k = 0; k < m; k++){
	    for(l = 0; l < m; l++){
	      ct[((j*m+l)*nm)+(i*m+k)] = tmp_mm[l*m+k];
	      tmp_mm[l*m+k] = 0.0; //zero out
	    }
	  }
	}
      }
      
      //
      //make C_str
      //
      for(i = 0; i < q; i++){
	for(j = 0; j < q; j++){
	  
	  zeros(tmp_mm, mm);
	  
	  for(k = 0; k < m; k++){
	    if(nPramPtr == 1)
	      (covModelObj->*cov1ParamPtr)(theta[k], tmp_mm[k*m+k], knotsD[j*q+i]);
	    else //i.e., 2 parameter matern
	      (covModelObj->*cov2ParamPtr)(theta[k], theta[m+k], tmp_mm[k*m+k], knotsD[j*q+i]);
	  }
	  
	  F77_NAME(dgemm)(ntran, ntran, &m, &m, &m, &one, A, &m, tmp_mm, &m, &zero, tmp_mm1, &m);
	  F77_NAME(dgemm)(ntran, ytran, &m, &m, &m, &one, tmp_mm1, &m, A, &m, &zero, tmp_mm, &m);
	  
	  for(k = 0; k < m; k++){
	    for(l = 0; l < m; l++){
	      C_str[((j*m+l)*qm)+(i*m+k)] = tmp_mm[l*m+k];
	      tmp_mm[l*m+k] = 0.0; //zero out
	    }
	  }
	}
      }
 
      detCand = 0.0;
      F77_NAME(dpotrf)(lower, &qm, C_str, &qm, &info); if(info != 0){error("mvPPCovInvDet: Cholesky failed (2)\n");}
      for(i = 0; i < qm; i++) detCand += 2.0*log(C_str[i*qm+i]);
      F77_NAME(dpotri)(lower, &qm, C_str, &qm, &info); if(info != 0){error("mvPPCovInvDet: Cholesky failed (3)\n");}
      
      F77_NAME(dsymm)(rside, lower, &nm, &qm, &one, C_str, &qm, ct, &nm, &zero, tmp_nmqm, &nm);

      //make \tild{w}
      F77_NAME(dgemv)(ntran, &nm, &qm, &one, tmp_nmqm, &nm, w_strCand, &incOne, &zero, wCand, &incOne);

      //
      //Likelihood with Jacobian   
      //
      logPostCand = 0.0;
      
      //
      //Jacobian and IW priors for K = A'A and Psi = L'L
      //

      if(betaPrior == "normal"){
	for(i = 0; i < p; i++){
	  logPostCand += dnorm(beta[i], betaMu[i], betaSd[i], 1);
	}
      }

      //AtA prior with jacob.
      logDetK = 0.0;
      SKtrace = 0.0;

      for(i = 0; i < m; i++) logDetK += 2*log(A[i*m+i]);
      
      //jacobian \sum_{i=1}^{m} (m-i+1)*log(a_ii)+log(a_ii)
      for(i = 0; i < m; i++) logPostCand += (m-i)*log(A[i*m+i])+log(A[i*m+i]);
      
      //get S*K^-1, already have the chol of K (i.e., A)
      F77_NAME(dpotri)(lower, &m, A, &m, &info); if(info != 0){cout << "c++ error: Cand A Cholesky inverse failed\n" << endl;}
      F77_NAME(dsymm)(rside, lower, &m, &m, &one, A, &m, KIW_S, &m, &zero, tmp_mm, &m);
      for(i = 0; i < m; i++){SKtrace += tmp_mm[i*m+i];}
      logPostCand += -0.5*(KIW_df+m+1)*logDetK - 0.5*SKtrace;

      for(i = 0; i < m; i++){
	logPostCand += log(theta[i] - phiUnif[i*2]) + log(phiUnif[i*2+1] - theta[i]); 
      
	if(covModel == "matern"){
	  logPostCand += log(theta[m+i] - nuUnif[i*2]) + log(nuUnif[i*2+1] - theta[m+i]);  
	}
      }
   
      F77_NAME(dgemv)(ntran, &nm, &p, &one, X, &nm, beta, &incOne, &zero, tmp_nm, &incOne);
     
      if(family == "binomial"){
	logPostCand += logit_logpost(nm, Y, tmp_nm, wCand);
      }else if(family == "poisson"){
	logPostCand += poisson_logpost(nm, Y, tmp_nm, wCand);
      }else{
	error("c++ error: family misspecification in spGLM\n");
      }

      //(-1/2) * tmp_n` *  C^-1 * tmp_n
      F77_NAME(dsymv)(lower, &qm, &one,  C_str, &qm, w_strCand, &incOne, &zero, tmp_qm, &incOne);
      logPostCand += -0.5*detCand-0.5*F77_NAME(ddot)(&qm, w_strCand, &incOne, tmp_qm, &incOne);

      //
      //MH accept/reject	
      //      
  
      //MH ratio with adjustment
      logMHRatio = logPostCand - logPostCurrent;
      
      if(runif(0.0,1.0) <= exp(logMHRatio)){
	F77_NAME(dcopy)(&nParams, candSpParams, &incOne, spParams, &incOne);
	F77_NAME(dcopy)(&qm, w_strCand, &incOne, w_strCurrent, &incOne);
	F77_NAME(dcopy)(&nm, wCand, &incOne, wCurrent, &incOne);
	logPostCurrent = logPostCand;
	accept++;
	batchAccept++;
      }
      
      /******************************
          Save samples and report
      *******************************/
      F77_NAME(dcopy)(&nParams, spParams, &incOne, &samples[s*nParams], &incOne);
      F77_NAME(dcopy)(&qm, w_strCurrent, &incOne, &w_str[s*qm], &incOne);
      F77_NAME(dcopy)(&nm, wCurrent, &incOne, &w[s*nm], &incOne);
      
      //report
      if(verbose){
	if(status == nReport){
	  Rprintf("Sampled: %i of %i, %3.2f%%\n", s, nSamples, 100.0*s/nSamples);
	  Rprintf("Report interval Metrop. Acceptance rate: %3.2f%%\n", 100.0*batchAccept/nReport);
	  Rprintf("Overall Metrop. Acceptance rate: %3.2f%%\n", 100.0*accept/s);
	  Rprintf("-------------------------------------------------\n");
          #ifdef Win32
	  R_FlushConsole();
          #endif
	  status = 0;
	  batchAccept = 0;
	}
      }
      status++;
      
      
      R_CheckUserInterrupt();
    }//end sample loop
    PutRNGstate();
    
    //final status report
    if(verbose){
      Rprintf("Sampled: %i of %i, %3.2f%%\n", s, nSamples, 100.0*s/nSamples);
    }
    Rprintf("-------------------------------------------------\n");
    #ifdef Win32
    R_FlushConsole();
    #endif

    //untransform variance variables
    for(s = 0; s < nSamples; s++){
 
      covTransInv(&samples[s*nParams+AIndx], &samples[s*nParams+AIndx], m);
   	
      for(i = 0; i < m; i++){
	samples[s*nParams+phiIndx+i] = logitInv(samples[s*nParams+phiIndx+i], phiUnif[i*2], phiUnif[i*2+1]);
	
	if(covModel == "matern"){
	  samples[s*nParams+nuIndx+i] = logitInv(samples[s*nParams+nuIndx+i], nuUnif[i*2], nuUnif[i*2+1]);
	}
      }
    }   


    //calculate acceptance rate
    REAL(accept_r)[0] = 100.0*accept/s;

    //make return object
    SEXP result, resultNames;
    
    int nResultListObjs = 4;

    PROTECT(result = allocVector(VECSXP, nResultListObjs)); nProtect++;
    PROTECT(resultNames = allocVector(VECSXP, nResultListObjs)); nProtect++;

   //samples
    SET_VECTOR_ELT(result, 0, samples_r);
    SET_VECTOR_ELT(resultNames, 0, mkChar("p.samples")); 

    SET_VECTOR_ELT(result, 1, accept_r);
    SET_VECTOR_ELT(resultNames, 1, mkChar("acceptance"));
    
    SET_VECTOR_ELT(result, 2, w_r);
    SET_VECTOR_ELT(resultNames, 2, mkChar("sp.effects"));
  
    SET_VECTOR_ELT(result, 3, w_str_r);
    SET_VECTOR_ELT(resultNames, 3, mkChar("sp.effects.knots"));

    namesgets(result, resultNames);
   
    //unprotect
    UNPROTECT(nProtect);
    
    return(result);
    
  }
}