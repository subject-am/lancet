#include "Variant.hh"

/****************************************************************************
** Variant.cc
**
** Class for storing basic variant information storage
**
*****************************************************************************/

/************************** COPYRIGHT ***************************************
**
** New York Genome Center
**
** SOFTWARE COPYRIGHT NOTICE AGREEMENT
** This software and its documentation are copyright (2016) by the New York
** Genome Center. All rights are reserved. This software is supplied without
** any warranty or guaranteed support whatsoever. The New York Genome Center
** cannot be responsible for its use, misuse, or functionality.
**
** Version: 1.0.0
** Author: Giuseppe Narzisi
**
*************************** /COPYRIGHT **************************************/


void Variant_t::printVCF() {
	//CHROM  POS     ID      REF     ALT     QUAL    FILTER  INFO    FORMAT  Pat4-FF-Normal-DNA      Pat4-FF-Tumor-DNA
	string ID = ".";
	string FILTER = "";

	string status = "?";
	char flag = bestState(ref_cov_normal,(alt_cov_normal_fwd+alt_cov_normal_rev),ref_cov_tumor,(alt_cov_tumor_fwd+alt_cov_tumor_rev));
	if(flag == 'T') { status = "SOMATIC"; }
	else if(flag == 'S') { status = "SHARED"; }
	else if(flag == 'N') { status = "NORMAL"; }
	else if(flag == 'E') { status = "NONE"; return; } // do not print varaints without support
	
	string INFO = status + ";FETS=" + dtos(fet_score);
	if(type=='I') { INFO += ";TYPE=ins"; }
	if(type=='D') { INFO += ";TYPE=del"; }
	if(type=='S') { INFO += ";TYPE=snv"; }
	
	double QUAL = fet_score;
	string FORMAT = "GT:AD:SC:DP";
	
	// apply filters
	
	int tot_alt_cov_tumor = alt_cov_tumor_fwd + alt_cov_tumor_rev;
	int tumor_cov = ref_cov_tumor + tot_alt_cov_tumor;
	double tumor_vaf = (tumor_cov == 0) ? 0 : ((double)tot_alt_cov_tumor/(double)tumor_cov);
	
	int tot_alt_cov_normal = alt_cov_normal_fwd + alt_cov_normal_rev;
	int normal_cov = ref_cov_normal + tot_alt_cov_normal;
	double normal_vaf = (normal_cov == 0) ? 0 : ((double)tot_alt_cov_normal/(double)normal_cov);
	
	if(fet_score < filters.minPhredFisher) { 
		if (FILTER.compare("") == 0) { FILTER = "LowFisherScore"; }
		else { FILTER += ";LowFisherScore"; }
	}
	if(normal_cov < filters.minCovNormal) { 
		if (FILTER.compare("") == 0) { FILTER = "LowCovNormal"; }
		else { FILTER += ";LowCovNormal"; }	
	}
	if(normal_cov > filters.maxCovNormal) { 
		if (FILTER.compare("") == 0) { FILTER = "HighCovNormal"; }
		else { FILTER += ";HighCovNormal"; }	
	}
	if(tumor_cov < filters.minCovTumor) { 
		if (FILTER.compare("") == 0) { FILTER = "LowCovTumor"; }
		else { FILTER += ";LowCovTumor"; }	
	}
	if(tumor_cov > filters.maxCovTumor) { 
		if (FILTER.compare("") == 0) { FILTER = "HighCovTumor"; }
		else { FILTER += ";HighCovTumor"; }	
	}
	if(tumor_vaf < filters.minVafTumor) { 
		if (FILTER.compare("") == 0) { FILTER = "LowVafTumor"; }
		else { FILTER += ";LowVafTumor"; }	
	}
	if(normal_vaf > filters.maxVafNormal) { 
		if (FILTER.compare("") == 0) { FILTER = "HighVafNormal"; }
		else { FILTER += ";HighVafNormal"; }	
	}
	if(tot_alt_cov_tumor < filters.minAltCntTumor) { 
		if (FILTER.compare("") == 0) { FILTER = "LowAltCntTumor"; }
		else { FILTER += ";LowAltCntTumor"; }	
	}
	if(tot_alt_cov_normal > filters.maxAltCntNormal) { 
		if (FILTER.compare("") == 0) { FILTER = "HighAltCntNormal"; }
		else { FILTER += ";HighAltCntNormal"; }
	}
	// snp specific filters
	if(type == 'S') { 
		if( (alt_cov_tumor_fwd < filters.minStrandBias) || (alt_cov_tumor_rev < filters.minStrandBias) ) { 
			if (FILTER.compare("") == 0) { FILTER = "StrandBias"; }
			else { FILTER += ";StrandBias"; }
		}
	}
		
	if(FILTER.compare("") == 0) { FILTER = "PASS"; }
	
	string NORMAL = GT_normal + ":" + itos(ref_cov_normal) + "," + itos(tot_alt_cov_normal) + ":" + itos(alt_cov_normal_fwd) + "," + itos(alt_cov_normal_rev) + ":" + itos(ref_cov_normal+tot_alt_cov_normal);
	string TUMOR = GT_tumor + ":" + itos(ref_cov_tumor) + "," + itos(tot_alt_cov_tumor) + ":" + itos(alt_cov_tumor_fwd) + "," + itos(alt_cov_tumor_rev) + ":" + itos(ref_cov_tumor+tot_alt_cov_tumor);
	
	cout << chr << "\t" << pos << "\t" << ID << "\t" << ref << "\t" << alt << "\t" << QUAL << "\t" << FILTER << "\t" << INFO << "\t" << FORMAT << "\t" << NORMAL << "\t" << TUMOR << endl;
}

// compute genotype info in VCF format (GT field)
//////////////////////////////////////////////////////////////
string Variant_t::genotype(int R, int A) {

	//0/0 - the sample is homozygous reference
	//0/1 - the sample is heterozygous, carrying 1 copy of each of the REF and ALT alleles
	//1/1 - the sample is homozygous alternate
	//1/2 - the sample is heterozygous, carrying 1 copy of the REF and 2 ALT alleles
	
	string GT = ""; // GT field in VCF format
	
	if(R>0 && A>0) { GT = "0/1"; }
	else if(R>0 && A==0) { GT = "0/0"; }
	else if(R==0 && A>0) { GT = "1/1"; }
	else if(R==0 && A==0) { GT = "."; }
	
	return GT;
}

void Variant_t::update() {
	
	double left = 0;
	double right = 0;
	double twotail = 0;
	FET_t fet;
	double prob = fet.kt_fisher_exact(ref_cov_normal, ref_cov_tumor, (alt_cov_normal_fwd+alt_cov_normal_rev), (alt_cov_tumor_fwd+alt_cov_tumor_rev), &left, &right, &twotail);
	if(prob == 1) { fet_score = 0; }
	else { fet_score = -10*log10(prob); }
	
	//compute genotype
	GT_normal = genotype(ref_cov_normal,(alt_cov_normal_fwd+alt_cov_normal_rev));
	GT_tumor = genotype(ref_cov_tumor,(alt_cov_tumor_fwd+alt_cov_tumor_rev));
}

// compute best state for the variant
//////////////////////////////////////////////////////////////
char Variant_t::bestState(int Rn, int An, int Rt, int At) {
	
	char ans = '?';
	if ( (An>0) && (At>0) ) { // shred
		ans = 'S';
	}
	else if ( (An==0) && (At>0) ) { // somatic
		ans = 'T';
	}
	else if ( (An>0) && (At==0) ) { // somatic
		ans = 'N';
	}
	else if ( (An==0) && (At==0) ) { // no support
		ans = 'E';
	}
	return ans;
}

string Variant_t::getSignature() {
		
	string ans = chr+":"+itos(pos)+":"+type+":"+itos(len)+":"+ref+":"+alt;
	//cerr << ans << endl;
	return ans;
}
