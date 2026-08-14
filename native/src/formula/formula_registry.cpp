#include "formula_engine_internal.hpp"

#include <initializer_list>

namespace tdx::formula_engine_detail {
namespace {

std::set<std::string> with_names(
    std::set<std::string> values,
    std::initializer_list<std::string_view> additions) {
    for (const auto name : additions) values.emplace(name);
    return values;
}

std::set<std::string> without_names(
    std::set<std::string> values,
    std::initializer_list<std::string_view> removals) {
    for (const auto name : removals) values.erase(std::string(name));
    return values;
}

std::set<std::string> union_names(
    std::set<std::string> values, const std::set<std::string>& additions) {
    values.insert(additions.begin(), additions.end());
    return values;
}

std::set<std::string> without_set(
    std::set<std::string> values, const std::set<std::string>& removals) {
    for (const auto& name : removals) values.erase(name);
    return values;
}

}  // namespace

const std::set<std::string> supported_functions{
    "ABS", "ACOS", "ALIGNRIGHT", "AMA", "ASIN", "ATAN", "AVEDEV", "BARSLAST", "BARSLASTS", "BARSSINCE", "BARSSINCEN", "BARSCOUNT", "CEILING", "CONST", "CONSTA", "COS", "COUNT", "COVAR", "CROSS",
    "BARSLASTCOUNT", "BARSNEXT", "BETA", "BETAEX", "BETWEEN", "CCI", "DATETOCUR", "DATETODAY", "DATETOTODAY", "DAYTODATE", "DCLOSE", "DHIGH", "DLOW", "DMA", "DOPEN", "DOWNNDAY", "DTPRICE", "DVOL", "DYNAINFO", "EMA",
    "DEVSQ", "EVERY", "EXIST", "EXISTR", "EXP", "EXPMA", "EXPMEMA", "EXTDATA_USER", "EXTERNSTR", "EXTERNVALUE", "FFTRANS", "FILTER", "FILTERX", "FINDHIGH", "FINDHIGHBARS", "FINDLOW", "FINDLOWBARS", "FLOOR", "FORCAST", "FRACPART", "SIGNALS_SYS", "SIGNALS_USER",
    "HHV", "HHVBARS", "HHVLLV", "HOD", "IF", "IFF", "IFN", "INCLUDED", "INCLUDEDV", "INTPART", "ISVALID", "LAST", "LLV", "LLVBARS", "LOD", "LOWRANGE", "LN",
    "BACKSET", "CON2STR", "COST", "COSTEX", "DRAWNUMBER_DIF", "FINANCE", "FINVALUE", "FINONE", "GPJYVALUE", "BKJYVALUE", "SCJYVALUE", "GPJYONE", "BKJYONE", "SCJYONE", "GPONEDAT", "LOG", "LONGCROSS", "LWINNER", "MA", "MAX", "MAX6", "MEMA", "MIN", "MIN6", "MOD", "MTM", "MULAR", "NDAY",
    "PEAK", "PEAKBARS", "PLOYLINE", "POW", "PSY", "RAND", "RANGE", "REF", "REFDATE", "REFV", "REFX", "REFXV", "RELATE", "REVERSE", "ROC", "ROUND", "ROUND2", "SAFESCORE", "SGN", "SHINESCORE", "SIGN", "SIN", "SLOPE", "SMA", "SPLIT", "SPLITBARS", "SQRT", "TAN", "TMA", "TOPRANGE", "TROUGH", "TROUGHBARS",
    "IVOLAT", "NEWSAR", "PPART", "PWINNER", "SAR", "SARTURN", "SECTOTIME", "STD", "STDDEV", "STDP", "SUM", "SUMBARS", "SUMBARSX", "TFILT", "TFILTER", "TIMETOSEC", "TQFLAG", "TR", "TTFILTER", "UPNDAY", "VALUEWHEN", "VAR", "VARP", "WINNER", "WMA", "WR", "XMA",
    "TDXASI", "TDXBB", "TDXBOLLM", "TDXKDJ", "TDXMCST", "TDXMSI", "TDXNDB", "TDXNVI", "TDXPAV", "TDXPAVE", "TDXPVI", "TDXSAR", "TDXSC", "TDXSSRP", "TDXVTY", "TDXWIDTH", "TDXXLPLBASE", "TDXZXNH", "L2_AMO", "L2_VOL", "L2_VOLNUM",
    "ZIG", "ZIGA", "ZTPRICE", "BUY", "SELL", "BUYSHORT", "SELLSHORT", "BUYSHORT_BUY",
    "SELL_SELLSHORT", "STICKLINE", "DRAWICON", "DRAWKLINE", "DRAWTEXT",
    "DRAWTEXT_FIX", "DRAWLINE", "DRAWSL", "DRAWNUMBER", "DRAWNUMBER_FIX", "PARTLINE", "DRAWBAND",
    "DRAWBMP", "DRAWGBK", "DRAWGBK_DIV", "DRAWRECTREL",
    "BETAVALUE", "DIVFACTOR", "DPZSCODE", "DPZSNAME", "DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS", "HYSJL", "HYSYL", "INDEXADV", "INDEXDEC", "MAINZSHQ", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT", "TOTALHQINFO", "TOTALMMPAMO", "UNDERCODE", "UNDERLYC",
    "BLOCKSETNUM", "CALCSTOCKINDEX", "CODELIKE", "FGBKZSCODE", "FINDSTR", "GETNAMEOFCODE", "GNBKZSCODE", "HORCALC", "INBLOCK", "INSORT", "INSUM", "NAMEINCLUDE", "NAMELIKE", "NOT", "RGB",
    "STR2CON", "STRCAT", "STRCAT6", "STRCMP", "STRLEN", "STRSPACE",
    "SUBSTR", "UPDOWN", "VAR2STR", "VARCAT", "VARCAT6",
    "IST0CODE", "ISSTCODE", "ISQUITCODE", "ISQHQQCODE",
    "ACTINVOL", "ACTOUTVOL", "AVGBIDPX", "AVGOFFERPX", "BIDCANCELVOL",
    "BIDORDERVOL", "CUR_BUYORDER", "CUR_SELLORDER", "ISBUYORDER",
    "OFFERCANCELVOL", "OFFERORDERVOL",
    "ISJYDATE", "LFS", "LOCALDAYNUM", "MACHINEDATE", "MACHINETIME", "MACHINEWEEK"
};

// TCalc's static registry contains these entries even though none of the 379
// bundled formulas needs them.  They extend the public inline-source
// interpreter rather than inflating built-in-library coverage.
const std::set<std::string> custom_formula_core_functions{
    "ACOS", "ASIN", "ATAN", "CONST", "CONSTA", "COS", "EXISTR",
    "FRACPART", "RANGE", "ROUND2", "SGN", "SIGN", "SIN", "TAN",
};

const std::set<std::string> custom_formula_transform_functions{
    "FFTRANS", "NEWSAR",
};

// TCalc opcode 1316 is the absolute-price-threshold Zig variant.  Unlike
// percentage ZIG, it confirms direction by a raw price difference and only
// promotes local extrema; its unfinished final leg is still linearly painted
// through the candidate extremum and the last bar.
const std::set<std::string> custom_formula_future_path_functions{
    "ZIGA",
};

// TCalc opcode 1181 resets the Microsoft CRT generator from _time64(0) once
// per RAND call.  An explicit evaluation seed preserves the same LCG while
// making audits and API replays deterministic.
const std::set<std::string> custom_formula_random_functions{
    "RAND",
};

// TCalc opcodes 1040..1044 merge adjacent bars into directional close-price
// segments.  Every completed segment OHLCV value is projected back onto all
// bars in that segment, so the five zero-argument entries are future functions.
// TDX source accepts these registry entries as bare automatic symbols; the
// native interpreter also accepts the equivalent empty-call spelling.
const std::set<std::string> custom_formula_directional_bar_functions{
    "DCLOSE", "DHIGH", "DLOW", "DOPEN", "DVOL",
};

// TCalc opcode 1348 broadcasts the exact adjustment flag supplied to
// CCalcBase setup.  The 0x052D K-line request uses the same 0/1/2 values for
// raw/front/back adjustment.
const std::set<std::string> custom_formula_adjustment_functions{
    "TQFLAG",
};

// TCalc opcodes 1378/1367 consume TdxW host callback types 122/168.
// They are zero-argument functions whose values are broadcast to every bar.
const std::set<std::string> custom_formula_host_calendar_functions{
    "ISJYDATE", "LOCALDAYNUM",
};

// TCalc opcode 1197 consumes the second float of TdxW host callback type 103:
// the date-effective circulating-capital series.  Its fast/slow retained
// turnover states are native recurrences, not ordinary EMA aliases.
const std::set<std::string> custom_formula_capital_turnover_functions{
    "LFS",
};

// TCalc opcodes 1330..1332 capture one CRT local-time snapshot and broadcast
// its DATE/HHMMSS/Sunday-zero weekday fields to the complete output series.
const std::set<std::string> custom_formula_machine_clock_functions{
    "MACHINEDATE", "MACHINETIME", "MACHINEWEEK",
};

const std::set<std::string> custom_formula_core_symbols{
    "BARSTATUS", "DAY", "MONTH", "TOTALBARSCOUNT", "WEEKDAY", "YEAR",
};

const std::set<std::string> custom_formula_sequence_statistics_functions{
    "BARSLASTS", "BARSSINCEN", "BETAEX", "COVAR", "FILTERX", "FINDHIGH",
    "FINDHIGHBARS", "FINDLOW", "FINDLOWBARS", "RELATE", "TMA", "XMA",
};

const std::set<std::string> custom_formula_rolling_variance_functions{
    "AMA", "DEVSQ", "HHVLLV", "HOD", "IFF", "IFN", "ISVALID", "LOD",
    "MAX6", "MIN6", "MULAR", "REFV", "STDP", "VAR", "VARP",
};

const std::set<std::string> custom_formula_benchmark_cumulative_functions{
    "BETA", "SUMBARSX",
};

const std::set<std::string> custom_formula_calendar_filter_functions{
    "ALIGNRIGHT", "DATETOCUR", "DAYTODATE", "SECTOTIME", "TFILT", "TFILTER",
    "TIMETOSEC", "TTFILTER",
};

const std::set<std::string> custom_formula_calendar_filter_symbols{
    "TIME2", "WEEKOFYEAR",
};

const std::set<std::string> custom_formula_security_string_functions{
    "CODELIKE", "FINDSTR", "NAMEINCLUDE", "NAMELIKE", "NOT", "STR2CON",
    "STRCAT6", "STRLEN", "STRSPACE", "SUBSTR", "UPDOWN", "VAR2STR",
    "VARCAT", "VARCAT6", "IST0CODE", "ISSTCODE", "ISQUITCODE",
    "ISQHQQCODE",
};

const std::set<std::string> custom_formula_security_string_symbols{
    "STKNAME",
};

// TCalc opcode 1252 asks TdxW host callback type 105 once, reads the signed
// 16-bit value at return offset 42, and broadcasts it to every bar.  TdxW
// derives that value from the public 7727/0x23F5 derivative-directory field.
const std::set<std::string> custom_formula_contract_metadata_symbols{
    "MULTIPLIER",
};

// TCalc opcodes 1333/1345..1347 request TdxW host record type 0xA3 and
// broadcast tdxstat.cfg column 3 or one component of packed column 23.
// TDX accepts these entries both as bare automatic symbols and empty calls.
const std::set<std::string> custom_formula_security_stat_functions{
    "BETAVALUE", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT",
};

// TCalc opcodes 1328/1344 select the current index or the security's
// configured 880xxx/881xxx industry index, then broadcast TdxW host fields
// type-120/+60 (PE) and type-163/+380 (PB MRQ).  The native context binds the
// public HYZT hyPE/hyPB leaf-industry record when it exists.
const std::set<std::string> custom_formula_industry_valuation_functions{
    "HYSJL", "HYSYL",
};

// TCalc opcodes 1201/1202 select the current security's native main index and
// copy the unsigned 16-bit advance/decline counts at K-line offsets 31/33.
// The same names are accepted as bare automatic symbols and empty calls.
const std::set<std::string> custom_formula_market_breadth_functions{
    "INDEXADV", "INDEXDEC",
};

// TCalc opcodes 1380..1383 are zero-argument quote aliases.  The first three
// share DYNAINFO selectors 7/14/17; DYNA_ZAS shares selector 24 and the public
// 0x053E rise-speed field.  Every value is broadcast to the complete vector.
const std::set<std::string> custom_formula_dynamic_quote_functions{
    "DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS",
};

// TCalc opcodes 1320/1323/1348/1357 expose the selected security's underlying
// instrument and exact native main index.  The three code/name values are
// string-pool handles; UNDERLYC is the date/time-aligned underlying CLOSE
// series.  All four registry entries are accepted as bare symbols and calls.
const std::set<std::string> custom_formula_security_relation_functions{
    "DPZSCODE", "DPZSNAME", "UNDERCODE", "UNDERLYC",
};

const std::set<std::string> custom_formula_security_relation_text_symbols{
    "DPZSCODE", "DPZSNAME", "UNDERCODE",
};

// TCalc opcode 1359 reads public type-164 corporate-action records.  Despite
// its broad help name, the DLL's factor is exactly 1 + bonus/transfer-per-10/10;
// cash dividends and rights terms are deliberately not folded into it.
const std::set<std::string> custom_formula_divfactor_functions{
    "DIVFACTOR",
};

// TCalc opcodes 1329/1008 use separate registry entries but copy the same
// float at offset 31 from every 35-byte host K-line record.  The host puts the
// time-chart cumulative average or derivative settlement price in that shared
// auxiliary slot, depending on the active market and period.
const std::set<std::string> custom_formula_kline_auxiliary_symbols{
    "QHJSJ", "ZSTJJ",
};

// TCalc routes these entries through TdxW command 8.  The native context
// builder reconstructs them from tdxzs3.cfg/tdxhy.cfg and
// infoharbor_block.dat, so they remain available without a broker session.
const std::set<std::string> custom_formula_block_metadata_functions{
    "BLOCKSETNUM", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE", "HORCALC", "INBLOCK",
};

// TCalc opcodes 1109/1246/1247 evaluate one selected output from another
// technical indicator. CALCSTOCKINDEX targets one security at the active
// period; INSORT/INSUM aggregate the output across a block.
const std::set<std::string> custom_formula_indicator_aggregate_functions{
    "CALCSTOCKINDEX", "INSORT", "INSUM",
};

const std::set<std::string> custom_formula_block_metadata_symbols{
    "FGBLOCK", "FGBLOCKNUM", "GNBLOCKNUM", "HYZSCODE", "ZSBLOCK",
    "ZSBLOCKNUM", "ZDBLOCK", "ZDBLOCKNUM", "ZHBLOCK", "ZHBLOCKNUM",
    "SIMIBLOCK",
};

// TCalc broadcasts these single-point host lookups across the whole formula
// vector.  Types 172/175 are backed by verified official gpcw/tdxgp packages;
// type 170 reads the terminal's optional local gp{sz,sh,bj}one.dat cache.
const std::set<std::string> custom_formula_single_point_functions{
    "BKJYONE", "FINONE", "GPJYONE", "GPONEDAT", "SCJYONE",
};

// TCalc opcodes 1031/1032 ask TdxW callback type 37 for one record from
// T0002/signals/extern_user.txt or extern_sys.txt.  Both functions inspect the
// final value of their two arguments and broadcast the selected record across
// the complete formula vector.
const std::set<std::string> custom_formula_external_signal_functions{
    "EXTERNSTR", "EXTERNVALUE",
};

// TCalc opcode 1298 requests TdxW callback selector 38.  TdxW reads the
// selected security's T0002/extdata/extdata_N.idx/.dat sequence and aligns its
// date/time/value triples using the caller-selected missing-point mode.
// Opcodes 1366/1030 use selectors 34/36 and read the local
// signals_sys_N.dat / signals_user_N/<market>_<code>.dat stores.  Their
// records are date-only and support the host's exact/forward-fill/zero modes.
const std::set<std::string> custom_formula_external_series_functions{
    "EXTDATA_USER", "SIGNALS_SYS", "SIGNALS_USER",
};

// These naked string symbols are copied from the TdxW callback type-167
// security snapshot.  LEVEL1HYBLOCK is reconstructed from the research
// industry hierarchy, while MAINBUSINESS comes from specgpext.txt.
const std::set<std::string> custom_formula_type167_text_symbols{
    "LEVEL1HYBLOCK", "MAINBUSINESS", "MOREHYBLOCK",
};

// TCalc opcodes 1356/1388 read callback type-167 offsets 51/67.  TdxW fills
// them from fields four/five of the same local specgpext.txt record used by
// MAINBUSINESS.
const std::set<std::string> custom_formula_security_score_functions{
    "SAFESCORE", "SHINESCORE",
};

// The remaining static TCalc registry entries are deliberately classified by
// the runtime facility they require.  Keeping this boundary as data prevents
// future audits from mistaking private/L2/trading/plugin callbacks for public
// OHLC-derived functions that are still waiting to be implemented.
const std::set<std::string> tcalc_registry_syntax_only_names{
    "AND", "OR", "TESTSKIP", "TOKENWORD",
};

const std::set<std::string> tcalc_registry_broker_private_signal_names{
    "SIGNALS_QS",
};

const std::set<std::string> tcalc_registry_level2_order_flow_names{
    "ACTINVOL", "ACTOUTVOL", "AVGBIDPX", "AVGOFFERPX", "BIDCANCELVOL",
    "BIDORDERVOL", "CUR_BUYORDER", "CUR_SELLORDER", "ISBUYORDER", "L2_VOL",
    "L2_VOLNUM", "OFFERCANCELVOL", "OFFERORDERVOL",
};

// These zero-argument account/strategy state symbols are read-only values in
// TCalc.  Most are obtained from host callback types 90/91; ISLAST*, LASTSIGNAL
// and the price/bar counters read the evaluator's private order-event history.
// The native toolkit never opens an account session, so it accepts them only
// as caller-owned explicit scalar or DATE|TIME series context.  ORDERBUY,
// ORDERSELL and CLOSEALLD/CLOSEALLK are deliberately excluded: they are action
// functions rather than observable state and must not be simulated by a
// read-only interpreter.
const std::set<std::string> tcalc_registry_live_trading_context_names{
    "BUYAVGPRICE", "BUYBARS", "BUYPRICE", "BUYPROFITLOSS", "BUYPOSITION",
    "BUYSHORTBARS", "BUYSHORTPRICE", "CANSELLGP", "CANUSEMONEY",
    "CANUSEPOSITION", "CLOSEPROFIT", "CURRENTEQUITY", "FEERATE",
    "FREEMONEY", "ISLASTBUY", "ISLASTBUYSHORT", "ISLASTSELL",
    "ISLASTSELLSHORT", "LASTSIGNAL", "MARGINRATE", "PREVIOUSEQUITY",
    "PROFITLOSS", "SELLAVGPRICE", "SELLBARS", "SELLPOSITION", "SELLPRICE",
    "SELLPROFITLOSS", "SELLSHORTBARS", "SELLSHORTPRICE", "TODAYBUY",
    "TODAYSELL", "TOTALAVGPRICE", "TOTALMARGIN", "TOTALPOSITION",
};

const std::set<std::string> tcalc_registry_live_trading_state_names = with_names(
    tcalc_registry_live_trading_context_names,
    {"CLOSEALLD", "CLOSEALLK", "ORDERBUY", "ORDERSELL"});

const std::set<std::string> tcalc_registry_plugin_callback_names{
    "TDXDLL1", "TDXDLL2", "TDXDLL3", "TDXDLL4", "TDXDLL5", "TDXDLL6",
    "TDXDLL7", "TDXDLL8", "TDXDLL9", "TDXDLL10", "USERFUNC0", "USERFUNC1",
    "USERFUNC2", "USERFUNC3", "USERFUNC4",
};

// These calls are accepted by the numeric interpreter, but their complete TDX
// meaning lives in the drawing/string runtime.  Keep the distinction explicit:
// a top-level drawing statement is harmless to numeric signals, while feeding
// its placeholder return value into a numeric output is not.
const std::set<std::string> presentation_only_functions{
    "STICKLINE", "DRAWICON", "DRAWKLINE", "DRAWTEXT", "DRAWTEXT_FIX",
    "DRAWNUMBER", "DRAWNUMBER_FIX", "DRAWNUMBER_DIF", "DRAWBAND", "DRAWBMP",
    "DRAWGBK", "DRAWGBK_DIV", "DRAWRECTREL", "DRAWSL"
};

// Supported string producers are materialized exactly in StringEnvironment.
// Their internal numeric pool handle remains opaque and is audited below.
const std::set<std::string> string_surrogate_functions{};
const std::set<std::string> numeric_surrogate_functions = with_names(
    presentation_only_functions, {
    "CON2STR", "DPZSCODE", "DPZSNAME", "EXTERNSTR", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE", "STRCAT", "STRCAT6", "STRSPACE", "SUBSTR", "UNDERCODE", "VAR2STR",
    "VARCAT", "VARCAT6", "RGB"});

// Functions that create a render primitive.  RGB/CON2STR/STRCAT are helpers
// whose exact values are consumed by these primitives, not primitives by
// themselves.
const std::set<std::string> render_ir_functions = with_names(
    presentation_only_functions, {"PARTLINE", "DRAWLINE", "PLOYLINE"});

const std::set<std::string> presentation_degraded_functions = with_names(
    render_ir_functions, {"CON2STR", "STRCAT", "RGB"});

const std::set<std::string> future_functions{
    "BACKSET", "BARSNEXT", "DATETOCUR", "DCLOSE", "DHIGH", "DLOW", "DOPEN", "DRAWLINE", "DVOL", "FFTRANS", "FILTERX", "INCLUDEDV", "PEAK", "PEAKBARS", "PLOYLINE", "REFX", "REFXV", "TROUGH",
    "TROUGHBARS", "XMA", "ZIG", "ZIGA", "TDXZXNH"
};

const std::set<std::string> external_functions{
    "ACTINVOL", "ACTOUTVOL", "AVGBIDPX", "AVGOFFERPX", "BETA", "BETAVALUE", "BIDCANCELVOL", "BIDORDERVOL", "BKJYVALUE", "BKJYONE", "BLOCKSETNUM", "CALCSTOCKINDEX", "COST", "COSTEX", "CUR_BUYORDER", "CUR_SELLORDER", "DIVFACTOR", "DPZSCODE", "DPZSNAME", "DYNAINFO", "DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS", "EXTDATA_USER", "EXTERNSTR", "EXTERNVALUE", "FINANCE", "FINVALUE", "FINONE", "FGBLOCK", "FGBKZSCODE", "GETNAMEOFCODE", "GNBKZSCODE", "GPJYVALUE", "GPJYONE", "GPONEDAT", "HORCALC", "HYSJL", "HYSYL", "INDEXADV", "INDEXDEC", "INBLOCK", "INSORT", "INSUM", "ISBUYORDER", "IVOLAT", "L2_AMO", "LWINNER", "MAINZSHQ", "OFFERCANCELVOL", "OFFERORDERVOL",
    "L2_VOL", "L2_VOLNUM", "SCJYVALUE", "SCJYONE", "SIGNALS_QS", "SIGNALS_SYS", "SIGNALS_USER", "SPLIT", "SPLITBARS",
    "PPART", "PWINNER", "TDXMCST", "TDXPAV", "TDXPAVE", "TDXSSRP", "WINNER",
    "IST0CODE", "ISSTCODE", "ISQUITCODE", "ISQHQQCODE",
    "ISJYDATE", "LFS", "LOCALDAYNUM", "SAFESCORE", "SHINESCORE", "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT",
    "TOTALHQINFO", "TOTALMMPAMO", "UNDERCODE", "UNDERLYC"
};

const std::set<std::string> market_symbols{
    "AMO", "AMOUNT", "C", "CLOSE", "H", "HIGH", "L", "LOW", "O", "OPEN",
    "V", "VOL", "VOLUME", "VOLINSTK", "CCL", "HKSHORTVOL", "QHJSJ", "ZSTJJ"
};

const std::set<std::string> constants{"FALSE", "TRUE", "DRAWNULL"};

const std::set<std::string> string_symbols{
    "CODE", "DPZSCODE", "DPZSNAME", "DYBLOCK", "FGBLOCK", "GNBLOCK", "HYBLOCK", "HYZSCODE",
    "LEVEL1HYBLOCK", "MAINBUSINESS", "MOREHYBLOCK", "SIMIBLOCK", "STKNAME",
    "UNDERCODE", "ZDBLOCK", "ZHBLOCK", "ZSBLOCK",
};

// TDX permits output references to another system indicator. These two
// references occur in the recovered library and are fully derivable from the
// current OHLC series, so they do not require an external market-data source.
const std::set<std::string> intrinsic_formula_symbols{"EXTERNAL#KDJ.J", "SAR.SAR"};

const std::set<std::string> builtin_symbols{
    "AUTOFILTER", "BARSTATUS", "CURRBARSCOUNT", "DATE", "DAY", "DAYSTOTODAY", "FROMOPEN", "HOUR", "HQCRBK", "ISLASTBAR",
    "MACHINEDATE", "MACHINETIME", "MACHINEWEEK", "MINDIFF", "MINUTE", "MONTH", "MTM", "PERIOD", "SETCODE", "TIME", "TIME2", "TOTALBARSCOUNT", "TOTALFZNUM", "TQFLAG", "WEEKDAY", "WEEKOFYEAR", "YEAR",
    "TR", "USEDDATANUM"
};

const std::set<std::string> external_symbols{
    "ADVANCE", "BUYVOL", "CAPITAL", "DECLINE", "HSL", "INDEXA", "INDEXC", "INDEXH",
    "INDEXL", "INDEXO", "INDEXV", "DYBLOCK", "FGBLOCK", "FGBLOCKNUM", "GNBLOCK",
    "GNBLOCKNUM", "HYBLOCK", "HYZSCODE", "HY_INDEXA", "HY_INDEXADV",
    "HY_INDEXC", "HY_INDEXDEC", "HY_INDEXH", "HY_INDEXL", "HY_INDEXO", "HY_INDEXV",
    "SELLVOL", "TOTALCAPITAL", "ZDBLOCK", "ZDBLOCKNUM", "ZHBLOCK", "ZHBLOCKNUM", "SIMIBLOCK", "ZSBLOCK", "ZSBLOCKNUM", "IST0CODE", "ISSTCODE", "ISQUITCODE",
    "ISQHQQCODE", "ISJYDATE", "LEVEL1HYBLOCK", "LOCALDAYNUM",
    "BETAVALUE", "DPZSCODE", "DPZSNAME", "DYNA_LB", "DYNA_NOW", "DYNA_ZAF", "DYNA_ZAS", "HYSJL", "HYSYL", "INDEXADV", "INDEXDEC", "MAINBUSINESS", "MOREHYBLOCK", "MULTIPLIER",
    "SHAPE_LONG", "SHAPE_MID", "SHAPE_SHORT", "UNDERCODE", "UNDERLYC"
};

// These TCalc symbols are not derivable from OHLCV or public L1 bars.  The
// interpreter can nevertheless evaluate them exactly when a caller supplies
// timestamp-keyed series captured from its own authorized data source.
const std::set<std::string> explicit_context_symbols = [] {
    std::set<std::string> names{
        "ACTINVOL", "ACTOUTVOL", "AVGBIDPX", "AVGOFFERPX", "BIDCANCELVOL",
        "BIDORDERVOL", "CUR_BUYORDER", "CUR_SELLORDER", "ISBUYORDER",
        "LARGEINTRDVOL", "LARGEOUTTRDVOL", "LARGETRDINNUM",
        "LARGETRDOUTNUM", "OFFERCANCELVOL", "OFFERORDERVOL", "TRADEINNUM",
        "TRADEOUTNUM", "TRADENUM", "HOST_TYPE120_SECURITY_CLASS_RAW",
        "HOST_EVALUATOR_MARKET_WORD_RAW"
    };
    names.insert(tcalc_registry_live_trading_context_names.begin(),
                 tcalc_registry_live_trading_context_names.end());
    return names;
}();

const std::set<std::string> context_external_dependencies = without_names(
    without_set(
        union_names(external_functions, external_symbols),
        tcalc_registry_level2_order_flow_names),
    {"L2_AMO", "SIGNALS_QS"});

const std::set<int> finance_context_ids{
    1, 2, 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 19, 20, 21, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 37, 38, 41, 42, 43, 44, 48,
    52, 53, 56, 88, 90, 91
};

const std::set<int> dynainfo_context_ids{
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 20, 21, 22, 23, 25,
    24, 26, 27, 28, 29, 37, 39, 58, 59, 88, 93
};

const std::set<std::string> render_event_functions = without_names(
    render_ir_functions, {"PARTLINE"});

}  // namespace tdx::formula_engine_detail
