# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
# find  source/i18n -maxdepth 1  ! -type d  | egrep  '\.(c|cpp)$' | \
# sort | sed "s/^\(.*\)$/      '\1',/"
    'icui18n_sources': [
      'source/i18n/affixpatternparser.cpp',
      'source/i18n/alphaindex.cpp',
      'source/i18n/anytrans.cpp',
      'source/i18n/astro.cpp',
      'source/i18n/basictz.cpp',
      'source/i18n/bocsu.cpp',
      'source/i18n/brktrans.cpp',
      'source/i18n/buddhcal.cpp',
      'source/i18n/calendar.cpp',
      'source/i18n/casetrn.cpp',
      'source/i18n/cecal.cpp',
      'source/i18n/chnsecal.cpp',
      'source/i18n/choicfmt.cpp',
      'source/i18n/coleitr.cpp',
      'source/i18n/collationbuilder.cpp',
      'source/i18n/collationcompare.cpp',
      'source/i18n/collation.cpp',
      'source/i18n/collationdatabuilder.cpp',
      'source/i18n/collationdata.cpp',
      'source/i18n/collationdatareader.cpp',
      'source/i18n/collationdatawriter.cpp',
      'source/i18n/collationfastlatinbuilder.cpp',
      'source/i18n/collationfastlatin.cpp',
      'source/i18n/collationfcd.cpp',
      'source/i18n/collationiterator.cpp',
      'source/i18n/collationkeys.cpp',
      'source/i18n/collationroot.cpp',
      'source/i18n/collationrootelements.cpp',
      'source/i18n/collationruleparser.cpp',
      'source/i18n/collationsets.cpp',
      'source/i18n/collationsettings.cpp',
      'source/i18n/collationtailoring.cpp',
      'source/i18n/collationweights.cpp',
      'source/i18n/coll.cpp',
      'source/i18n/compactdecimalformat.cpp',
      'source/i18n/coptccal.cpp',
      'source/i18n/cpdtrans.cpp',
      'source/i18n/csdetect.cpp',
      'source/i18n/csmatch.cpp',
      'source/i18n/csr2022.cpp',
      'source/i18n/csrecog.cpp',
      'source/i18n/csrmbcs.cpp',
      'source/i18n/csrsbcs.cpp',
      'source/i18n/csrucode.cpp',
      'source/i18n/csrutf8.cpp',
      'source/i18n/curramt.cpp',
      'source/i18n/currfmt.cpp',
      'source/i18n/currpinf.cpp',
      'source/i18n/currunit.cpp',
      'source/i18n/dangical.cpp',
      'source/i18n/datefmt.cpp',
      'source/i18n/dcfmtsym.cpp',
      'source/i18n/decContext.c',
      'source/i18n/decfmtst.cpp',
      'source/i18n/decimalformatpattern.cpp',
      'source/i18n/decimfmt.cpp',
      'source/i18n/decimfmtimpl.cpp',
      'source/i18n/decNumber.c',
      'source/i18n/digitaffix.cpp',
      'source/i18n/digitaffixesandpadding.cpp',
      'source/i18n/digitformatter.cpp',
      'source/i18n/digitgrouping.cpp',
      'source/i18n/digitinterval.cpp',
      'source/i18n/digitlst.cpp',
      'source/i18n/dtfmtsym.cpp',
      'source/i18n/dtitvfmt.cpp',
      'source/i18n/dtitvinf.cpp',
      'source/i18n/dtptngen.cpp',
      'source/i18n/dtrule.cpp',
      'source/i18n/esctrn.cpp',
      'source/i18n/ethpccal.cpp',
      'source/i18n/fmtable_cnv.cpp',
      'source/i18n/fmtable.cpp',
      'source/i18n/format.cpp',
      'source/i18n/fphdlimp.cpp',
      'source/i18n/fpositer.cpp',
      'source/i18n/funcrepl.cpp',
      'source/i18n/gender.cpp',
      'source/i18n/gregocal.cpp',
      'source/i18n/gregoimp.cpp',
      'source/i18n/hebrwcal.cpp',
      'source/i18n/identifier_info.cpp',
      'source/i18n/indiancal.cpp',
      'source/i18n/inputext.cpp',
      'source/i18n/islamcal.cpp',
      'source/i18n/japancal.cpp',
      'source/i18n/locdspnm.cpp',
      'source/i18n/measfmt.cpp',
      'source/i18n/measunit.cpp',
      'source/i18n/measure.cpp',
      'source/i18n/msgfmt.cpp',
      'source/i18n/name2uni.cpp',
      'source/i18n/nfrs.cpp',
      'source/i18n/nfrule.cpp',
      'source/i18n/nfsubs.cpp',
      'source/i18n/nortrans.cpp',
      'source/i18n/nultrans.cpp',
      'source/i18n/numfmt.cpp',
      'source/i18n/numsys.cpp',
      'source/i18n/olsontz.cpp',
      'source/i18n/persncal.cpp',
      'source/i18n/pluralaffix.cpp',
      'source/i18n/plurfmt.cpp',
      'source/i18n/plurrule.cpp',
      'source/i18n/precision.cpp',
      'source/i18n/quant.cpp',
      'source/i18n/quantityformatter.cpp',
      'source/i18n/rbnf.cpp',
      'source/i18n/rbt.cpp',
      'source/i18n/rbt_data.cpp',
      'source/i18n/rbt_pars.cpp',
      'source/i18n/rbt_rule.cpp',
      'source/i18n/rbt_set.cpp',
      'source/i18n/rbtz.cpp',
      'source/i18n/regexcmp.cpp',
      'source/i18n/regeximp.cpp',
      'source/i18n/regexst.cpp',
      'source/i18n/regextxt.cpp',
      'source/i18n/region.cpp',
      'source/i18n/reldatefmt.cpp',
      'source/i18n/reldtfmt.cpp',
      'source/i18n/rematch.cpp',
      'source/i18n/remtrans.cpp',
      'source/i18n/repattrn.cpp',
      'source/i18n/rulebasedcollator.cpp',
      'source/i18n/scientificnumberformatter.cpp',
      'source/i18n/scriptset.cpp',
      'source/i18n/search.cpp',
      'source/i18n/selfmt.cpp',
      'source/i18n/sharedbreakiterator.cpp',
      'source/i18n/simpletz.cpp',
      'source/i18n/smallintformatter.cpp',
      'source/i18n/smpdtfmt.cpp',
      'source/i18n/smpdtfst.cpp',
      'source/i18n/sortkey.cpp',
      'source/i18n/standardplural.cpp',
      'source/i18n/strmatch.cpp',
      'source/i18n/strrepl.cpp',
      'source/i18n/stsearch.cpp',
      'source/i18n/taiwncal.cpp',
      'source/i18n/timezone.cpp',
      'source/i18n/titletrn.cpp',
      'source/i18n/tmunit.cpp',
      'source/i18n/tmutamt.cpp',
      'source/i18n/tmutfmt.cpp',
      'source/i18n/tolowtrn.cpp',
      'source/i18n/toupptrn.cpp',
      'source/i18n/translit.cpp',
      'source/i18n/transreg.cpp',
      'source/i18n/tridpars.cpp',
      'source/i18n/tzfmt.cpp',
      'source/i18n/tzgnames.cpp',
      'source/i18n/tznames.cpp',
      'source/i18n/tznames_impl.cpp',
      'source/i18n/tzrule.cpp',
      'source/i18n/tztrans.cpp',
      'source/i18n/ucal.cpp',
      'source/i18n/ucln_in.cpp',
      'source/i18n/ucol.cpp',
      'source/i18n/ucoleitr.cpp',
      'source/i18n/ucol_res.cpp',
      'source/i18n/ucol_sit.cpp',
      'source/i18n/ucsdet.cpp',
      'source/i18n/ucurr.cpp',
      'source/i18n/udat.cpp',
      'source/i18n/udateintervalformat.cpp',
      'source/i18n/udatpg.cpp',
      'source/i18n/ufieldpositer.cpp',
      'source/i18n/uitercollationiterator.cpp',
      'source/i18n/ulocdata.c',
      'source/i18n/umsg.cpp',
      'source/i18n/unesctrn.cpp',
      'source/i18n/uni2name.cpp',
      'source/i18n/unum.cpp',
      'source/i18n/unumsys.cpp',
      'source/i18n/upluralrules.cpp',
      'source/i18n/uregexc.cpp',
      'source/i18n/uregex.cpp',
      'source/i18n/uregion.cpp',
      'source/i18n/usearch.cpp',
      'source/i18n/uspoof_build.cpp',
      'source/i18n/uspoof_conf.cpp',
      'source/i18n/uspoof.cpp',
      'source/i18n/uspoof_impl.cpp',
      'source/i18n/uspoof_wsconf.cpp',
      'source/i18n/utf16collationiterator.cpp',
      'source/i18n/utf8collationiterator.cpp',
      'source/i18n/utmscale.c',
      'source/i18n/utrans.cpp',
      'source/i18n/valueformatter.cpp',
      'source/i18n/visibledigits.cpp',
      'source/i18n/vtzone.cpp',
      'source/i18n/vzone.cpp',
      'source/i18n/windtfmt.cpp',
      'source/i18n/winnmfmt.cpp',
      'source/i18n/wintzimpl.cpp',
      'source/i18n/zonemeta.cpp',
      'source/i18n/zrule.cpp',
      'source/i18n/ztrans.cpp',
    ],
    'icuuc_sources': [
# find  source/common -maxdepth 1  ! -type d  | egrep  '\.(c|cpp)$' | \
# sort | sed "s/^\(.*\)$/      '\1',/"
      'source/common/appendable.cpp',
      'source/common/bmpset.cpp',
      'source/common/brkeng.cpp',
      'source/common/brkiter.cpp',
      'source/common/bytestream.cpp',
      'source/common/bytestriebuilder.cpp',
      'source/common/bytestrie.cpp',
      'source/common/bytestrieiterator.cpp',
      'source/common/caniter.cpp',
      'source/common/chariter.cpp',
      'source/common/charstr.cpp',
      'source/common/cmemory.c',
      'source/common/cstring.c',
      'source/common/cwchar.c',
      'source/common/dictbe.cpp',
      'source/common/dictionarydata.cpp',
      'source/common/dtintrv.cpp',
      'source/common/errorcode.cpp',
      'source/common/filteredbrk.cpp',
      'source/common/filterednormalizer2.cpp',
      'source/common/icudataver.c',
      'source/common/icuplug.cpp',
      'source/common/listformatter.cpp',
      'source/common/loadednormalizer2impl.cpp',
      'source/common/locavailable.cpp',
      'source/common/locbased.cpp',
      'source/common/locdispnames.cpp',
      'source/common/locid.cpp',
      'source/common/loclikely.cpp',
      'source/common/locmap.c',
      'source/common/locresdata.cpp',
      'source/common/locutil.cpp',
      'source/common/messagepattern.cpp',
      'source/common/normalizer2.cpp',
      'source/common/normalizer2impl.cpp',
      'source/common/normlzr.cpp',
      'source/common/parsepos.cpp',
      'source/common/patternprops.cpp',
      'source/common/pluralmap.cpp',
      'source/common/propname.cpp',
      'source/common/propsvec.c',
      'source/common/punycode.cpp',
      'source/common/putil.cpp',
      'source/common/rbbi.cpp',
      'source/common/rbbidata.cpp',
      'source/common/rbbinode.cpp',
      'source/common/rbbirb.cpp',
      'source/common/rbbiscan.cpp',
      'source/common/rbbisetb.cpp',
      'source/common/rbbistbl.cpp',
      'source/common/rbbitblb.cpp',
      'source/common/resbund_cnv.cpp',
      'source/common/resbund.cpp',
      'source/common/resource.cpp',
      'source/common/ruleiter.cpp',
      'source/common/schriter.cpp',
      'source/common/serv.cpp',
      'source/common/servlk.cpp',
      'source/common/servlkf.cpp',
      'source/common/servls.cpp',
      'source/common/servnotf.cpp',
      'source/common/servrbf.cpp',
      'source/common/servslkf.cpp',
      'source/common/sharedobject.cpp',
      'source/common/simplepatternformatter.cpp',
      'source/common/stringpiece.cpp',
      'source/common/stringtriebuilder.cpp',
      'source/common/uarrsort.c',
      'source/common/ubidi.c',
      'source/common/ubidiln.c',
      'source/common/ubidi_props.c',
      'source/common/ubidiwrt.c',
      'source/common/ubrk.cpp',
      'source/common/ucase.cpp',
      'source/common/ucasemap.cpp',
      'source/common/ucasemap_titlecase_brkiter.cpp',
      'source/common/ucat.c',
      'source/common/uchar.c',
      'source/common/ucharstriebuilder.cpp',
      'source/common/ucharstrie.cpp',
      'source/common/ucharstrieiterator.cpp',
      'source/common/uchriter.cpp',
      'source/common/ucln_cmn.cpp',
      'source/common/ucmndata.c',
      'source/common/ucnv2022.cpp',
      'source/common/ucnv_bld.cpp',
      'source/common/ucnvbocu.cpp',
      'source/common/ucnv.c',
      'source/common/ucnv_cb.c',
      'source/common/ucnv_cnv.c',
      'source/common/ucnv_ct.c',
      'source/common/ucnvdisp.c',
      'source/common/ucnv_err.c',
      'source/common/ucnv_ext.cpp',
      'source/common/ucnvhz.c',
      'source/common/ucnv_io.cpp',
      'source/common/ucnvisci.c',
      'source/common/ucnvlat1.c',
      'source/common/ucnv_lmb.c',
      'source/common/ucnvmbcs.cpp',
      'source/common/ucnvscsu.c',
      'source/common/ucnvsel.cpp',
      'source/common/ucnv_set.c',
      'source/common/ucnv_u16.c',
      'source/common/ucnv_u32.c',
      'source/common/ucnv_u7.c',
      'source/common/ucnv_u8.c',
      'source/common/ucol_swp.cpp',
      'source/common/udata.cpp',
      'source/common/udatamem.c',
      'source/common/udataswp.c',
      'source/common/uenum.c',
      'source/common/uhash.c',
      'source/common/uhash_us.cpp',
      'source/common/uidna.cpp',
      'source/common/uinit.cpp',
      'source/common/uinvchar.c',
      'source/common/uiter.cpp',
      'source/common/ulist.c',
      'source/common/ulistformatter.cpp',
      'source/common/uloc.cpp',
      'source/common/uloc_keytype.cpp',
      'source/common/uloc_tag.c',
      'source/common/umapfile.c',
      'source/common/umath.c',
      'source/common/umutex.cpp',
      'source/common/unames.cpp',
      'source/common/unifiedcache.cpp',
      'source/common/unifilt.cpp',
      'source/common/unifunct.cpp',
      'source/common/uniset_closure.cpp',
      'source/common/uniset.cpp',
      'source/common/uniset_props.cpp',
      'source/common/unisetspan.cpp',
      'source/common/unistr_case.cpp',
      'source/common/unistr_case_locale.cpp',
      'source/common/unistr_cnv.cpp',
      'source/common/unistr.cpp',
      'source/common/unistr_props.cpp',
      'source/common/unistr_titlecase_brkiter.cpp',
      'source/common/unormcmp.cpp',
      'source/common/unorm.cpp',
      'source/common/uobject.cpp',
      'source/common/uprops.cpp',
      'source/common/uresbund.cpp',
      'source/common/ures_cnv.c',
      'source/common/uresdata.cpp',
      'source/common/uresource.cpp',
      'source/common/usc_impl.c',
      'source/common/uscript.c',
      'source/common/uscript_props.cpp',
      'source/common/uset.cpp',
      'source/common/usetiter.cpp',
      'source/common/uset_props.cpp',
      'source/common/ushape.cpp',
      'source/common/usprep.cpp',
      'source/common/ustack.cpp',
      'source/common/ustrcase.cpp',
      'source/common/ustrcase_locale.cpp',
      'source/common/ustr_cnv.cpp',
      'source/common/ustrenum.cpp',
      'source/common/ustrfmt.c',
      'source/common/ustring.cpp',
      'source/common/ustr_titlecase_brkiter.cpp',
      'source/common/ustrtrns.cpp',
      'source/common/ustr_wcs.cpp',
      'source/common/utext.cpp',
      'source/common/utf_impl.c',
      'source/common/util.cpp',
      'source/common/util_props.cpp',
      'source/common/utrace.c',
      'source/common/utrie2_builder.cpp',
      'source/common/utrie2.cpp',
      'source/common/utrie.cpp',
      'source/common/uts46.cpp',
      'source/common/utypes.c',
      'source/common/uvector.cpp',
      'source/common/uvectr32.cpp',
      'source/common/uvectr64.cpp',
      'source/common/wintz.c',
    ],
    'icuio_sources' : [
      "source/io/locbund.cpp",
      "source/io/sprintf.c",
      "source/io/sscanf.c",
      "source/io/ucln_io.cpp",
      "source/io/ufile.c",
      "source/io/ufmt_cmn.c",
      "source/io/uprintf.cpp",
      "source/io/uprntf_p.c",
       "source/io/uscanf.c",
      "source/io/uscanf_p.c",
      "source/io/ustdio.c",
      "source/io/ustream.cpp",
    ]
  }
}
