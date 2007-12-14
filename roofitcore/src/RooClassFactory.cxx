/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   DK, David Kirkby,    UC Irvine,         dkirkby@uci.edu                 *
 *                                                                           *
 * Copyright (c) 2000-2005, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/

// -- CLASS DESCRIPTION [MISC] --
// RooClassFactory is a clase like TTree::MakeClass() that generates
// skeleton code for RooAbsPdf and RooAbsReal functions given
// a list of input parameter names
//

#include "RooFit.h"

#include "RooClassFactory.h"
#include "RooClassFactory.h"
#include "RooAbsReal.h"
#include "RooAbsCategory.h"
#include "RooArgList.h"
#include "RooMsgService.h"
#include "TInterpreter.h"
#include <fstream>
#include <vector>
#include <string>

using namespace std ;

ClassImp(RooClassFactory) 
;


RooClassFactory::RooClassFactory()
{
}


RooClassFactory::~RooClassFactory() 
{
}


Bool_t RooClassFactory::makeAndCompilePdf(const char* name, const char* expression, const RooArgList& vars, const char* intExpression) 
{
  string realArgNames,catArgNames ;
  TIterator* iter = vars.createIterator() ;
  RooAbsArg* arg ;
  while((arg=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsReal*>(arg)) {
      if (realArgNames.size()>0) realArgNames += "," ;
      realArgNames += arg->GetName() ;
    } else if (dynamic_cast<RooAbsCategory*>(arg)) {
      if (catArgNames.size()>0) catArgNames += "," ;
      catArgNames += arg->GetName() ;
    } else {
      oocoutE((RooAbsArg*)0,InputArguments) << "RooClassFactory::makeAndCompilePdf ERROR input argument " << arg->GetName() 
					      << " is neither RooAbsReal nor RooAbsCategory and is ignored" << endl ;
    }
  }
  delete iter ;

  Bool_t ret = makePdf(name,realArgNames.c_str(),catArgNames.c_str(),expression,intExpression?kTRUE:kFALSE,kFALSE,intExpression) ;
  if (ret) {
    return ret ;
  }
 
  if (gInterpreter->GetRootMapFiles()==0) {
    gInterpreter->EnableAutoLoading() ;
  }

  TInterpreter::EErrorCode ecode;
  gInterpreter->ProcessLineSynch(Form(".L %s.cxx+",name),&ecode) ;
  return (ecode!=TInterpreter::kNoError) ;
}


Bool_t RooClassFactory::makeAndCompileFunction(const char* name, const char* expression, const RooArgList& vars, const char* intExpression) 
{
  string realArgNames,catArgNames ;
  TIterator* iter = vars.createIterator() ;
  RooAbsArg* arg ;
  while((arg=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsReal*>(arg)) {
      if (realArgNames.size()>0) realArgNames += "," ;
      realArgNames += arg->GetName() ;
    } else if (dynamic_cast<RooAbsCategory*>(arg)) {
      if (catArgNames.size()>0) catArgNames += "," ;
      catArgNames += arg->GetName() ;
    } else {
      oocoutE((RooAbsArg*)0,InputArguments) << "RooClassFactory::makeAndCompileFunction ERROR input argument " << arg->GetName() 
					    << " is neither RooAbsReal nor RooAbsCategory and is ignored" << endl ;
    }
  }
  delete iter ;

  Bool_t ret = makeFunction(name,realArgNames.c_str(),catArgNames.c_str(),expression,intExpression?kTRUE:kFALSE,intExpression) ;
  if (ret) {
    return ret ;
  }

  if (gInterpreter->GetRootMapFiles()==0) {
    gInterpreter->EnableAutoLoading() ;
  }

  TInterpreter::EErrorCode ecode;
  gInterpreter->ProcessLineSynch(Form(".L %s.cxx+",name),&ecode) ;
  return (ecode!=TInterpreter::kNoError) ;
}


RooAbsReal* RooClassFactory::makeFunctionInstance(const char* name, const char* expression, const RooArgList& vars, const char* intExpression) 
{
  if (gInterpreter->GetRootMapFiles()==0) {
    gInterpreter->EnableAutoLoading() ;
  }

  // Construct unique class name for this function expression
  string className = Form("Roo%sClass",name) ;

  // Use class factory to compile and link specialized function
  Bool_t error = makeAndCompileFunction(className.c_str(),expression,vars,intExpression) ;

  // Check that class was created OK
  if (error) {
    RooErrorHandler::softAbort() ;
  }

  // Create CINT line that instantiates specialized object
  string line = Form("new %s(\"%s\",\"%s\"",className.c_str(),name,name) ;

  // Make list of pointer values (represented in hex ascii) to be passed to cint  
  // Note that the order of passing arguments must match the convention in which
  // the class code is generated: first all reals, then all categories

  TIterator* iter = vars.createIterator() ;
  string argList ;
  // First pass the RooAbsReal arguments in the list order
  RooAbsArg* var ;
  while((var=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsReal*>(var)) {
      argList += Form(",*((RooAbsReal*)%p)",var) ;
    }
  }
  iter->Reset() ;
  // Next pass the RooAbsCategory arguments in the list order
  while((var=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsCategory*>(var)) {
      argList += Form(",*((RooAbsCategory*)%p)",var) ;
    }
  }
  delete iter ;

  line += argList + ") " ;

  // Let CINT instantiate specialized formula
  return (RooAbsReal*) gInterpreter->ProcessLineSynch(line.c_str()) ;
}


RooAbsPdf* RooClassFactory::makePdfInstance(const char* name, const char* expression, 
					    const RooArgList& vars, const char* intExpression)
{
  if (gInterpreter->GetRootMapFiles()==0) {
    gInterpreter->EnableAutoLoading() ;
  }

  // Construct unique class name for this function expression
  string className = Form("Roo%sClass",name) ;

  // Use class factory to compile and link specialized function
  Bool_t error = makeAndCompilePdf(className.c_str(),expression,vars,intExpression) ;

  // Check that class was created OK
  if (error) {
    RooErrorHandler::softAbort() ;
  }

  // Create CINT line that instantiates specialized object
  string line = Form("new %s(\"%s\",\"%s\"",className.c_str(),name,name) ;

  // Make list of pointer values (represented in hex ascii) to be passed to cint  
  // Note that the order of passing arguments must match the convention in which
  // the class code is generated: first all reals, then all categories

  TIterator* iter = vars.createIterator() ;
  string argList ;
  // First pass the RooAbsReal arguments in the list order
  RooAbsArg* var ;
  while((var=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsReal*>(var)) {
      argList += Form(",*((RooAbsReal*)%p)",var) ;
    }
  }
  iter->Reset() ;
  // Next pass the RooAbsCategory arguments in the list order
  while((var=(RooAbsArg*)iter->Next())) {
    if (dynamic_cast<RooAbsCategory*>(var)) {
      argList += Form(",*((RooAbsCategory*)%p)",var) ;
    }
  }
  delete iter ;

  line += argList + ") " ;

  // Let CINT instantiate specialized formula
  return (RooAbsPdf*) gInterpreter->ProcessLineSynch(line.c_str()) ;
}


Bool_t RooClassFactory::makePdf(const char* name, const char* argNames, const char* catArgNames, const char* expression, 
				Bool_t hasAnaInt, Bool_t hasIntGen, const char* intExpression) 
{
  return makeClass("RooAbsPdf",name,argNames,catArgNames,expression,hasAnaInt,hasIntGen,intExpression) ;
}

Bool_t RooClassFactory::makeFunction(const char* name, const char* argNames, const char* catArgNames, const char* expression, Bool_t hasAnaInt, const char* intExpression) 
{
  return makeClass("RooAbsReal",name,argNames,catArgNames,expression,hasAnaInt,kFALSE,intExpression) ;
}

Bool_t RooClassFactory::makeClass(const char* baseName, const char* className, const char* realArgNames, const char* catArgNames, 
				  const char* expression,  Bool_t hasAnaInt, Bool_t hasIntGen, const char* intExpression)
{
  // Check that arguments were given
  if (!baseName) {
    oocoutE((TObject*)0,InputArguments) << "RooClassFactory::makeClass: ERROR: a base class name must be given" << endl ;
    return kTRUE ;
  }

  if (!className) {
    oocoutE((TObject*)0,InputArguments) << "RooClassFactory::makeClass: ERROR: a class name must be given" << endl ;
    return kTRUE ;
  }

  if ((!realArgNames || !*realArgNames) && (!catArgNames || !*catArgNames)) {
    oocoutE((TObject*)0,InputArguments) << "RooClassFactory::makeClass: ERROR: A list of input argument names must be given" << endl ;
    return kTRUE ;
  }

  if (intExpression && !hasAnaInt) {
    oocoutE((TObject*)0,InputArguments) << "RooClassFactory::makeClass: ERROR no analytical integration code requestion, but expression for analytical integral provided" << endl ;
    return kTRUE ;
  }

  // Parse comma separated list of argument names into list of strings
  vector<string> alist ;
  vector<bool> isCat ;

  if (realArgNames && *realArgNames) {
    char* buf = new char[strlen(realArgNames)+1] ;
    strcpy(buf,realArgNames) ;
    char* token = strtok(buf,",") ;
    while(token) {
      alist.push_back(token) ;
      isCat.push_back(false) ;
      token = strtok(0,",") ;
    }
    delete[] buf ;
  }
  if (catArgNames && *catArgNames) {
    char* buf = new char[strlen(catArgNames)+1] ;
    strcpy(buf,catArgNames) ;
    char* token = strtok(buf,",") ;
    while(token) {
      alist.push_back(token) ;
      isCat.push_back(true) ;      
      token = strtok(0,",") ;
    }
    delete[] buf ;
  }
  
  TString impFileName(className), hdrFileName(className) ;
  impFileName += ".cxx" ;
  hdrFileName += ".h" ;

  TString ifdefName(className) ;
  ifdefName.ToUpper() ;
  
  ofstream hf(hdrFileName) ;
  hf << "/*****************************************************************************" << endl
     << " * Project: RooFit                                                           *" << endl
     << " *                                                                           *" << endl
     << " * Copyright (c) 2000-2007, Regents of the University of California          *" << endl
     << " *                          and Stanford University. All rights reserved.    *" << endl
     << " *                                                                           *" << endl
     << " * Redistribution and use in source and binary forms,                        *" << endl
     << " * with or without modification, are permitted according to the terms        *" << endl
     << " * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *" << endl
     << " *****************************************************************************/" << endl
     << endl
     << "#ifndef " << ifdefName << endl
     << "#define " << ifdefName << endl
     << "" << endl     
     << "#include \"" << baseName << ".h\"" << endl
     << "#include \"RooRealProxy.h\"" << endl
     << "#include \"RooCategoryProxy.h\"" << endl
     << "#include \"RooAbsReal.h\"" << endl
     << "#include \"RooAbsCategory.h\"" << endl
     << " " << endl
     << "class " << className << " : public " << baseName << " {" << endl
     << "public:" << endl
     << "  " << className << "() {} ; " << endl 
     << "  " << className << "(const char *name, const char *title," << endl ;
  
  // Insert list of input arguments
  unsigned int i ;
  for (i=0 ; i<alist.size() ; i++) { 
    if (!isCat[i]) {
      hf << "	      RooAbsReal& _" ;
    } else {
      hf << "	      RooAbsCategory& _" ;
    }
    hf << alist[i] ;
    if (i==alist.size()-1) {
      hf << ");" << endl ;
    } else {
      hf << "," << endl ;
    }    
  }
  
  hf << "  " << className << "(const " << className << "& other, const char* name=0) ;" << endl
     << "  virtual TObject* clone(const char* newname) const { return new " << className << "(*this,newname); }" << endl
     << "  inline virtual ~" << className << "() { }" << endl
     << endl ;

  if (hasAnaInt) {
    hf << "  Int_t getAnalyticalIntegral(RooArgSet& allVars, RooArgSet& analVars, const char* rangeName=0) const ;" << endl
       << "  Double_t analyticalIntegral(Int_t code, const char* rangeName=0) const ;" << endl
       << "" << endl ;
  }

  if (hasIntGen) {
     hf << "  Int_t getGenerator(const RooArgSet& directVars, RooArgSet &generateVars, Bool_t staticInitOK=kTRUE) const;" << endl
	<< "  void initGenerator(Int_t code) {} ; // optional pre-generation initialization" << endl
	<< "  void generateEvent(Int_t code);" << endl
	<< endl ;
  }
       
  hf << "protected:" << endl
     << "" << endl ;

  // Insert list of input arguments
  for (i=0 ; i<alist.size() ; i++) { 
    if (!isCat[i]) {
      hf << "  RooRealProxy " << alist[i] << " ;" << endl ;
    } else {
      hf << "  RooCategoryProxy " << alist[i] << " ;" << endl ;
    }
  }
  
  hf << "  " << endl 
     << "  Double_t evaluate() const ;" << endl
     << "" << endl
     << "private:" << endl
     << "" << endl
     << "  ClassDef(" << className << ",1) // Your description goes here..." << endl
     << "};" << endl
     << " " << endl
     << "#endif" << endl ;


  ofstream cf(impFileName) ;

  cf << " /***************************************************************************** " << endl 
     << "  * Project: RooFit                                                           * " << endl 
     << "  *                                                                           * " << endl 
     << "  * Copyright (c) 2000-2005, Regents of the University of California          * " << endl 
     << "  *                          and Stanford University. All rights reserved.    * " << endl 
     << "  *                                                                           * " << endl 
     << "  * Redistribution and use in source and binary forms,                        * " << endl 
     << "  * with or without modification, are permitted according to the terms        * " << endl 
     << "  * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             * " << endl 
     << "  *****************************************************************************/ " << endl 
     << endl 

     << " // -- CLASS DESCRIPTION [PDF] -- " << endl 
     << " // Your description goes here... " << endl 
     << endl 

     << " #include \"Riostream.h\" " << endl 
     << endl 

     << " #include \"" << className << ".h\" " << endl 
     << " #include \"RooAbsReal.h\" " << endl 
     << " #include \"RooAbsCategory.h\" " << endl 
     << endl 

     << " ClassImp(" << className << ") " << endl 
     << endl 

     << " " << className << "::" << className << "(const char *name, const char *title, " << endl ;

  // Insert list of proxy constructors
  for (i=0 ; i<alist.size() ; i++) { 
    if (!isCat[i]) {
      cf << "                        RooAbsReal& _" << alist[i] ;
    } else {
      cf << "                        RooAbsCategory& _" << alist[i] ;
    }
    if (i<alist.size()-1) {
      cf << "," ;
    } else {
      cf << ") :" ;
    }
    cf << endl ;
  }

  // Insert base class constructor
  cf << "   " << baseName << "(name,title), " << endl ;
  
  // Insert list of proxy constructors
  for (i=0 ; i<alist.size() ; i++) { 
    cf << "   " << alist[i] << "(\"" << alist[i] << "\",\"" << alist[i] << "\",this,_" << alist[i] << ")" ;
    if (i<alist.size()-1) {
      cf << "," ;
    }
    cf << endl ;
  }
  
  cf << " { " << endl 
     << " } " << endl 
     << endl 
     << endl 

     << " " << className << "::" << className << "(const " << className << "& other, const char* name) :  " << endl 
     << "   " << baseName << "(other,name), " << endl ;

  for (i=0 ; i<alist.size() ; i++) { 
    cf << "   " << alist[i] << "(\"" << alist[i] << "\",this,other." << alist[i] << ")" ;
    if (i<alist.size()-1) {
      cf << "," ;
    }
    cf << endl ;
  }

  cf << " { " << endl 
     << " } " << endl 
     << endl 
     << endl 
     << endl 

     << " Double_t " << className << "::evaluate() const " << endl 
     << " { " << endl 
     << "   // ENTER EXPRESSION IN TERMS OF VARIABLE ARGUMENTS HERE " << endl 
     << "   return " << expression << " ; " << endl
     << " } " << endl 
     << endl 
     << endl 
     << endl ;

  if (hasAnaInt) {

    vector<string> intObs ;
    vector<string> intExpr ;
    // Parse analytical integration expression if provided
    // Expected form is observable:expression,observable,observable:expression;[...]
    if (intExpression && *intExpression) {
      char* buf = new char[strlen(intExpression)+1] ;
      strcpy(buf,intExpression) ;
      char* ptr = strtok(buf,":") ;
      while(ptr) {
	intObs.push_back(ptr) ;
	intExpr.push_back(strtok(0,";")) ;
	ptr = strtok(0,":") ;
      }
      delete[] buf ;
    }
    
    cf << " Int_t " << className << "::getAnalyticalIntegral(RooArgSet& allVars, RooArgSet& analVars, const char* /*rangeName*/) const  " << endl 
       << " { " << endl 
       << "   // LIST HERE OVER WHICH VARIABLES ANALYTICAL INTEGRATION IS SUPPORTED, " << endl 
       << "   // ASSIGN A NUMERIC CODE FOR EACH SUPPORTED (SET OF) PARAMETERS " << endl 
       << "   // THE EXAMPLE BELOW ASSIGNS CODE 1 TO INTEGRATION OVER VARIABLE X" << endl 
       << "   // YOU CAN ALSO IMPLEMENT MORE THAN ONE ANALYTICAL INTEGRAL BY REPEATING THE matchArgs " << endl
       << "   // EXPRESSION MULTIPLE TIMES" << endl 
       << endl  ;

    if (intObs.size()>0) {
      for (UInt_t i=0 ; i<intObs.size() ; i++) {
	cf << "   if (matchArgs(allVars,analVars," << intObs[i] << ")) return " << i+1 << " ; " << endl ;
      }
    } else {
      cf << "   // if (matchArgs(allVars,analVars,x)) return 1 ; " << endl ;
    }

    cf << "   return 0 ; " << endl 
       << " } " << endl 
       << endl 
       << endl 
       << endl 

       << " Double_t " << className << "::analyticalIntegral(Int_t code, const char* rangeName) const  " << endl 
       << " { " << endl 
       << "   // RETURN ANALYTICAL INTEGRAL DEFINED BY RETURN CODE ASSIGNED BY getAnalyticalIntegral" << endl
       << "   // THE MEMBER FUNCTION x.min(rangeName) AND x.max(rangeName) WILL RETURN THE INTEGRATION" << endl
       << "   // BOUNDARIES FOR EACH OBSERVABLE x" << endl 
       << endl ;

    if (intObs.size()>0) {
      for (UInt_t i=0 ; i<intObs.size() ; i++) {
	cf << "   if (code==" << i+1 << ") { return (" << intExpr[i] << ") ; } " << endl ;
      }      
    } else {
      cf << "   // assert(code==1) ; " << endl 
	 << "   // return (x.max(rangeName)-x.min(rangeName)) ; " << endl ;
    }

    cf << "   return 0 ; " << endl
       << " } " << endl 
       << endl 
       << endl 
       << endl ;
  }
  
  if (hasIntGen) {
    cf << " Int_t " << className << "::getGenerator(const RooArgSet& directVars, RooArgSet &generateVars, Bool_t /*staticInitOK*/) const " << endl 
       << " { " << endl 
       << "   // LIST HERE OVER WHICH VARIABLES INTERNAL GENERATION IS SUPPORTED, " << endl 
       << "   // ASSIGN A NUMERIC CODE FOR EACH SUPPORTED (SET OF) PARAMETERS " << endl 
       << "   // THE EXAMPLE BELOW ASSIGNS CODE 1 TO INTEGRATION OVER VARIABLE X" << endl 
       << "   // YOU CAN ALSO IMPLEMENT MORE THAN ONE GENERATOR CONFIGURATION BY REPEATING THE matchArgs " << endl
       << "   // EXPRESSION MULTIPLE TIMES. IF THE FLAG staticInitOK IS TRUE THEN IT IS SAFE TO PRECALCULATE " << endl 
       << "   // INTERMEDIATE QUANTITIES IN initGenerator(), IF IT IS NOT SET THEN YOU SHOULD NOT ADVERTISE" << endl
       << "   // ANY GENERATOR METHOD THAT RELIES ON PRECALCULATIONS IN initGenerator()" << endl 
       << endl 
       << "   // if (matchArgs(directVars,generateVars,x)) return 1 ;   " << endl 
       << "   return 0 ; " << endl 
       << " } " << endl 
       << endl 
       << endl 
       << endl 

       << " void " << className << "::generateEvent(Int_t code) " << endl 
       << " { " << endl 
       << "   // GENERATE SET OF OBSERVABLES DEFINED BY RETURN CODE ASSIGNED BY getGenerator()" << endl
       << "   // RETURN THE GENERATED VALUES BY ASSIGNING THEM TO THE PROXY DATA MEMBERS THAT" << endl
       << "   // REPRESENT THE CHOSEN OBSERVABLES" << endl
       << endl 
       << "   // assert(code==1) ; " << endl 
       << "   // x = 0 ; " << endl 
       << "   return; " << endl 
       << " } " << endl 
       << endl 
       << endl 
       << endl ;
  }

  
  return kFALSE ;
}


