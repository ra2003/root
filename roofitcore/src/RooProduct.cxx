/*****************************************************************************
 * Project: RooFit                                                           *
 * Package: RooFitCore                                                       *
 * @(#)root/roofitcore:$Id$
 * Authors:                                                                  *
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu       *
 *   GR, Gerhard Raven,   VU Amsterdan,     graven@nikhef.nl                 *
 *                                                                           *
 * Copyright (c) 2000-2007, Regents of the University of California          *
 *                          and Stanford University. All rights reserved.    *
 *                                                                           *
 * Redistribution and use in source and binary forms,                        *
 * with or without modification, are permitted according to the terms        *
 * listed in LICENSE (http://roofit.sourceforge.net/license.txt)             *
 *****************************************************************************/

// -- CLASS DESCRIPTION [REAL] --
//
// RooProduct calculates the product of a set of RooAbsReal terms.
//


#include "RooFit.h"

#include "Riostream.h"
#include "Riostream.h"
#include <math.h>
#include <vector>
#include <utility>
#include <memory>

#include "RooProduct.h"
#include "RooNameReg.h"
#include "RooAbsReal.h"
#include "RooAbsCategory.h"
#include "RooErrorHandler.h"
#include "RooMsgService.h"

using namespace std ;

ClassImp(RooProduct)
;

class RooProduct::ProdMap : public  std::vector<std::pair<RooArgSet*,RooArgSet*> > {} ;

// Namespace with helper functions that have STL stuff that we don't want to expose to CINT
namespace {
  typedef RooProduct::ProdMap::iterator RPPMIter ;
  std::pair<RPPMIter,RPPMIter> findOverlap2nd(RPPMIter i, RPPMIter end)  ;
  void dump_map(ostream& os, RPPMIter i, RPPMIter end) ;
}


RooProduct::RooProduct() :
  _compRIter (0),
  _compCIter (0)
{
}


RooProduct::~RooProduct()
{
  if (_compRIter) {
    delete _compRIter ;
  }

  if (_compCIter) {
    delete _compCIter ;
  }
}


RooProduct::RooProduct(const char* name, const char* title, const RooArgSet& prodSet) :
  RooAbsReal(name, title),
  _compRSet("compRSet","Set of real product components",this),
  _compCSet("compCSet","Set of category product components",this),
  _compRIter( _compRSet.createIterator() ),
  _compCIter( _compCSet.createIterator() ),
  _cacheMgr(this,10)
{
  TIterator* compIter = prodSet.createIterator() ;
  RooAbsArg* comp ;
  while((comp = (RooAbsArg*)compIter->Next())) {
    if (dynamic_cast<RooAbsReal*>(comp)) {
      _compRSet.add(*comp) ;
    } else if (dynamic_cast<RooAbsCategory*>(comp)) {
      _compCSet.add(*comp) ;
    } else {
      coutE(InputArguments) << "RooProduct::ctor(" << GetName() << ") ERROR: component " << comp->GetName() 
			    << " is not of type RooAbsReal or RooAbsCategory" << endl ;
      RooErrorHandler::softAbort() ;
    }
  }
  delete compIter ;
}



RooProduct::RooProduct(const RooProduct& other, const char* name) :
  RooAbsReal(other, name), 
  _compRSet("compRSet",this,other._compRSet),
  _compCSet("compCSet",this,other._compCSet),
  _compRIter(_compRSet.createIterator()),
  _compCIter(_compCSet.createIterator()),
  _cacheMgr(other._cacheMgr,this)
{
  // Copy constructor
}


Bool_t RooProduct::forceAnalyticalInt(const RooAbsArg& dep) const
{
  // Force internal handling of integration of given observable
  // if any of the product terms depend on it.
  _compRIter->Reset() ;
  RooAbsReal* rcomp ;
  Bool_t depends(kFALSE);
  while((rcomp=(RooAbsReal*)_compRIter->Next())&&!depends) {
        depends = rcomp->dependsOn(dep);
  }
  return depends ;
}


RooProduct::ProdMap* RooProduct::groupProductTerms(const RooArgSet& allVars) const 
{
  ProdMap* map = new ProdMap ;

  // Do we have any terms which do not depend on the
  // on the variables we integrate over?
  RooAbsReal* rcomp ; _compRIter->Reset() ;
  RooArgSet *indep = new RooArgSet();
  while((rcomp=(RooAbsReal*)_compRIter->Next())) {
    if( !rcomp->dependsOn(allVars) ) indep->add(*rcomp);
  }
  if (indep->getSize()!=0) {
    map->push_back( std::make_pair(new RooArgSet(),indep) );
  }

  // Map observables -> functions ; start with individual observables
  TIterator *allVarsIter = allVars.createIterator() ;
  RooAbsReal* var ;
  while((var=(RooAbsReal*)allVarsIter->Next())) {
    RooArgSet *vars  = new RooArgSet(); vars->add(*var);
    RooArgSet *comps = new RooArgSet();
    RooAbsReal* rcomp ; 
    
    _compRIter->Reset() ;
    while((rcomp=(RooAbsReal*)_compRIter->Next())) {
      if( rcomp->dependsOn(*var) ) comps->add(*rcomp);
    }
    map->push_back( std::make_pair(vars,comps) );
  }

  // Merge groups with overlapping dependents
  Bool_t overlaps;
  do {
    std::pair<ProdMap::iterator,ProdMap::iterator> i = findOverlap2nd(map->begin(),map->end());
    overlaps = (i.first!=i.second);
    if (overlaps) {
      i.first->first->add(*i.second->first);
      i.first->second->add(*i.second->second);
      delete i.second->first;
      delete i.second->second;
      map->erase(i.second);
    }
  } while (overlaps);
  
  // check that we have all variables to be integrated over on the LHS
  // of the map, and all terms in the product do appear on the RHS
  int nVar=0; int nFunc=0;
  for (ProdMap::iterator i = map->begin();i!=map->end();++i) {
    nVar+=i->first->getSize();
    nFunc+=i->second->getSize();
  }
  assert(nVar==allVars.getSize());
  assert(nFunc==_compRSet.getSize());
  return map;
}


Int_t
RooProduct::getPartIntList(const RooArgSet* iset, const char *isetRange) const
{
  // check if we already have integrals for this combination of factors
  Int_t sterileIndex(-1);
  CacheElem* cache = (CacheElem*) _cacheMgr.getObj(iset,iset,&sterileIndex,RooNameReg::ptr(isetRange));
  if (cache!=0) {
    Int_t code = _cacheMgr.lastIndex();
    return code;
  }
  
  ProdMap* map = groupProductTerms(*iset);

  cxcoutD(Integration) << "RooProduct::getPartIntList(" << GetName() << ") groupProductTerms returned map" ;
  if (dologD(Integration)) {
    dump_map(ccoutD(Integration),map->begin(),map->end()); 
    ccoutD(Integration) << endl;
  }
  
  // did we find any factorizable terms?
  if (map->size()<2) return -1; // RRI caller will zero analVars if return code = 0....

  cache = new CacheElem();

  for (ProdMap::const_iterator i = map->begin();i!=map->end();++i) {
    RooAbsReal *term(0);
    if (i->second->getSize()>1) { // create a RooProd for this subexpression
      const char *name = makeFPName("SUBPROD_",*i->second);
      term = new RooProduct(name,name,*i->second);
      cache->_ownedList.addOwned(*term);
      cxcoutD(Integration) << "RooProduct::getPartIntList(" << GetName() << ") created subexpression " << term->GetName() << endl;
    } else {
      assert(i->second->getSize()==1);
      auto_ptr<TIterator> j( i->second->createIterator() );
      term = (RooAbsReal*)j->Next();
    }
    assert(term!=0);
    if (i->first->getSize()==0) { // check whether we need to integrate over this term or not...
      cache->_prodList.add(*term);
      cxcoutD(Integration) << "RooProduct::getPartIntList(" << GetName() << ") adding simple factor " << term->GetName() << endl;
    } else {
      RooAbsReal *integral = term->createIntegral(*i->first,isetRange);
      cache->_prodList.add(*integral);
      cache->_ownedList.addOwned(*integral);
      cxcoutD(Integration) << "RooProduct::getPartIntList(" << GetName() << ") adding integral for " << term->GetName() << " : " << integral->GetName() << endl;
    }
  }
  // add current set-up to cache, and return index..
  Int_t code = _cacheMgr.setObj(iset,iset,(RooAbsCacheElement*)cache,RooNameReg::ptr(isetRange));

  cxcoutD(Integration) << "RooProduct::getPartIntList(" << GetName() << ") created list " << cache->_prodList << " with code " << code+1 << endl
		       << " for iset=" << *iset << " @" << iset << " range: " << (isetRange?isetRange:"<none>") << endl ;

  delete map ;
  return code;
}

Int_t RooProduct::getAnalyticalIntegralWN(RooArgSet& allVars, RooArgSet& analVars,
                                            const RooArgSet* normSet,
                                            const char* rangeName) const
{
  // figure out if we can factorize the integration into a product of integrals

  if (_forceNumInt) return 0 ;

  // Declare that we can analytically integrate all requested observables
  // (basically, we will take care of the problem, and delegate where required)
  assert(normSet==0);
  assert(analVars.getSize()==0);
  analVars.add(allVars) ;
  Int_t code = getPartIntList(&analVars,rangeName)+1;
  return code ;
}

Double_t RooProduct::analyticalIntegral(Int_t code, const char* rangeName) const
{
  // note: rangeName implicit encoded in code: see _cacheMgr.setObj in getPartIntList...
  CacheElem *cache = (CacheElem*) _cacheMgr.getObjByIndex(code-1);
  if (cache==0) { 
    // cache got sterilized, trigger repopulation of this slot, then try again...
    std::auto_ptr<RooArgSet> vars( getParameters(RooArgSet()) );
    std::auto_ptr<RooArgSet> iset(  _cacheMgr.nameSet2ByIndex(code-1)->select(*vars) );
    Int_t code2 = getPartIntList(iset.get(),rangeName)+1;
    assert(code==code2); // must have revived the right (sterilized) slot...
    return analyticalIntegral(code2,rangeName);
  }
  assert(cache!=0);
  
  return calculate(cache->_prodList);
}

Double_t RooProduct::calculate(const RooArgList& partIntList) const
{
  RooAbsReal *term(0);
  Double_t val=1;
  std::auto_ptr<TIterator> i( partIntList.createIterator() );
  while((term=(RooAbsReal*)i->Next())) {
    double x = term->getVal();
    val*= x;
  }
  return val;
}

const char* RooProduct::makeFPName(const char *pfx,const RooArgSet& terms) const
{
    static TString pname;
    pname = pfx;
    std::auto_ptr<TIterator> i( terms.createIterator() );
    RooAbsArg *arg;
    Bool_t first(kTRUE);
    while((arg=(RooAbsArg*)i->Next())) {
        if (first) { first=kFALSE;}
        else pname.Append("_X_");
        pname.Append(arg->GetName());
    }
    return pname.Data();
}


Double_t RooProduct::evaluate() const 
{
  Double_t prod(1) ;

  _compRIter->Reset() ;
  RooAbsReal* rcomp ;
  const RooArgSet* nset = _compRSet.nset() ;
  while((rcomp=(RooAbsReal*)_compRIter->Next())) {
    prod *= rcomp->getVal(nset) ;
  }
  
  _compCIter->Reset() ;
  RooAbsCategory* ccomp ;
  while((ccomp=(RooAbsCategory*)_compCIter->Next())) {
    prod *= ccomp->getIndex() ;
  }
  return prod ;
}

RooProduct::CacheElem::~CacheElem() 
{
}

RooArgList RooProduct::CacheElem::containedArgs(Action) 
{
  RooArgList ret(_ownedList) ;
  return ret ;
}


namespace {

std::pair<RPPMIter,RPPMIter> findOverlap2nd(RPPMIter i, RPPMIter end) 
{
  for (; i!=end; ++i) for ( RPPMIter j(i+1); j!=end; ++j) {
    if (i->second->overlaps(*j->second)) {
      return std::make_pair(i,j);
    }
  }
  return std::make_pair(end,end);
}

  
void dump_map(ostream& os, RPPMIter i, RPPMIter end) 
{
  Bool_t first(kTRUE);
  os << " [ " ;
  for(; i!=end;++i) {
    if (first) { first=kFALSE; }
    else { os << " , " ; }
    os << *(i->first) << " -> " << *(i->second) ;
  }
  os << " ] " ; 
}

}




