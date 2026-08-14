#include "formula_engine_test_support.hpp"

namespace formula_engine_test {
namespace {

void set_close_series(tdx::Json& document,
                      const std::vector<double>& chronological_values) {
    auto& bars = document["bars"].as_array();
    require(bars.size() == chronological_values.size(),
            "native scalar fixture size");
    for (std::size_t index = 0; index < chronological_values.size(); ++index) {
        auto& bar = bars[bars.size() - 1 - index];
        const double value = chronological_values[index];
        bar["open"] = value;
        bar["high"] = value;
        bar["low"] = value;
        bar["close"] = value;
        bar["amount"] = std::abs(value) * 10000.0;
    }
}

void require_null(const tdx::Json& result, std::size_t index,
                  const std::string& output, const std::string& message) {
    require(point_value(result, index, output).is_null(), message);
}

void require_number(const tdx::Json& result, std::size_t index,
                    const std::string& output, double expected,
                    const std::string& message) {
    const auto& value = point_value(result, index, output);
    require(value.is_number() && value.as_number() == expected, message);
}

}  // namespace

void run_native_scalar_tests() {
    auto round_sample = sample(5);
    set_close_series(round_sample,
                     {16777217.0, 1.497, -1.497, 0.4969, 0.497});
    const auto native_round = tdx::evaluate_formula_source_document(
        round_sample, "R:ROUND(CLOSE);", {}, "NATIVEROUND");
    require_number(native_round, 0, "R", 16777216.0,
                   "ROUND first narrows its operand through raw float");
    require_number(native_round, 1, "R", 2.0,
                   "ROUND uses the native positive 0.503 bias");
    require_number(native_round, 2, "R", -2.0,
                   "ROUND uses the native negative 0.503 bias");
    require_number(native_round, 3, "R", 0.0,
                   "ROUND keeps values below the native bias boundary at zero");
    require_number(native_round, 4, "R", 1.0,
                   "ROUND crosses the native bias boundary after float landing");

    auto integer_sample = sample(5);
    set_close_series(integer_sample,
                     {16777217.0, 1.000005, 1.00002, -1.000005, -1.00002});
    const auto native_integer = tdx::evaluate_formula_source_document(
        integer_sample, "C:CEILING(CLOSE);F:FLOOR(CLOSE);", {},
        "NATIVEINTEGERBOUNDS");
    require_number(native_integer, 0, "C", 16777216.0,
                   "CEILING narrows large operands through raw float");
    require_number(native_integer, 0, "F", 16777216.0,
                   "FLOOR narrows large operands through raw float");
    require_number(native_integer, 1, "C", 1.0,
                   "CEILING keeps a positive value inside native tolerance");
    require_number(native_integer, 2, "C", 2.0,
                   "CEILING increments beyond native tolerance");
    require_number(native_integer, 3, "F", -1.0,
                   "FLOOR keeps a negative value inside native tolerance");
    require_number(native_integer, 4, "F", -2.0,
                   "FLOOR decrements beyond native tolerance");

    auto missing_sample = sample(5);
    set_close_series(missing_sample, {0.2, 1.2, 2.2, 3.2, 4.2});
    const auto native_missing = tdx::evaluate_formula_source_document(
        missing_sample,
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,"
        "IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE));"
        "R:ROUND(X);C:CEILING(X);F:FLOOR(X);",
        {}, "NATIVEINTEGERMISSING");
    require_null(native_missing, 0, "R",
                 "ROUND preserves a leading canonical sentinel");
    require_null(native_missing, 0, "C",
                 "CEILING skips a leading canonical sentinel");
    require_null(native_missing, 0, "F",
                 "FLOOR skips a leading canonical sentinel");
    require_null(native_missing, 3, "R",
                 "ROUND checks the canonical sentinel on every bar");
    require_number(native_missing, 3, "C", -2147483648.0,
                   "CEILING converts a post-start sentinel through native i32");
    require_number(native_missing, 3, "F", -2147483648.0,
                   "FLOOR converts a post-start sentinel through native i32");

    const auto native_overflow = tdx::evaluate_formula_source_document(
        sample(1),
        "R:ROUND(2147483648);C:CEILING(2147483648);F:FLOOR(2147483648);",
        {}, "NATIVEINTEGEROVERFLOW");
    require_number(native_overflow, 0, "R", -2147483648.0,
                   "ROUND maps float overflow through the native i32 sentinel");
    require_number(native_overflow, 0, "C", -2147483648.0,
                   "CEILING maps float overflow through the native i32 sentinel");
    require_number(native_overflow, 0, "F", -2147483648.0,
                   "FLOOR maps float overflow through the native i32 sentinel");

    auto const_sample = sample(4);
    set_close_series(const_sample, {1.0, 2.0, 3.0, 16777217.0});
    const auto native_const = tdx::evaluate_formula_source_document(
        const_sample, "C:CONST(CLOSE);", {}, "NATIVECONST");
    for (std::size_t index = 0; index < 4; ++index)
        require_number(native_const, index, "C", 16777216.0,
                       "CONST broadcasts the final raw-f32 source value");

    const auto native_const_missing = tdx::evaluate_formula_source_document(
        sample(3), "C:CONST(DRAWNULL);", {}, "NATIVECONSTMISSING");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(native_const_missing, index, "C",
                     "CONST broadcasts a final canonical sentinel as missing");

    auto consta_sample = sample(4);
    set_close_series(consta_sample, {10.0, 20.0, 30.0, 16777217.0});
    const auto native_consta = tdx::evaluate_formula_source_document(
        consta_sample,
        "R:CONSTA(CLOSE,1.99999999);F:CONSTA(CLOSE,0);"
        "M:CONSTA(CLOSE,DRAWNULL);O:CONSTA(CLOSE,99);",
        {}, "NATIVECONSTA");
    for (std::size_t index = 0; index < 4; ++index) {
        require_number(native_consta, index, "R", 20.0,
                       "CONSTA narrows the final offset to raw f32 before truncation");
        require_number(native_consta, index, "F", 16777216.0,
                       "CONSTA broadcasts the selected raw-f32 source value");
        require_number(native_consta, index, "M", 16777216.0,
                       "CONSTA maps a missing final offset through native i32 and clamps to zero");
        require_number(native_consta, index, "O", 10.0,
                       "CONSTA clamps an oversized final offset to the oldest source slot");
    }

    const auto native_round2 = tdx::evaluate_formula_source_document(
        sample(3),
        "P:ROUND2(1.234,1.99999999);N:ROUND2(-1.234,1.99999999);"
        "Z:ROUND2(1.497,-1);M:ROUND2(DRAWNULL,2);",
        {}, "NATIVEROUND2");
    const double positive_round2 = static_cast<double>(
        static_cast<float>(123.0F / 100.0F));
    const double negative_round2 = static_cast<double>(
        static_cast<float>(-123.0F / 100.0F));
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_round2, index, "P", positive_round2,
                       "ROUND2 narrows precision to raw f32 before truncation");
        require_number(native_round2, index, "N", negative_round2,
                       "ROUND2 applies the same f32 precision to negative values");
        require_number(native_round2, index, "Z", 2.0,
                       "ROUND2 clamps a negative precision to zero");
        require_null(native_round2, index, "M",
                     "ROUND2 preserves a canonical source sentinel");
    }

    const auto native_range = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,"
        "IF(CURRBARSCOUNT=1,16777217,5));"
        "L:=IF(CURRBARSCOUNT>=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,0,"
        "IF(CURRBARSCOUNT=2,DRAWNULL,16777214)));"
        "U:=IF(CURRBARSCOUNT=5 OR CURRBARSCOUNT=2,DRAWNULL,"
        "IF(CURRBARSCOUNT=1,16777218,10));R:RANGE(X,L,U);",
        {}, "NATIVERANGE");
    require_null(native_range, 0, "R",
                 "RANGE preserves the jointly missing leading bound region");
    require_number(native_range, 1, "R", 1.0,
                   "RANGE starts when either bound first becomes available");
    require_number(native_range, 2, "R", 0.0,
                   "RANGE evaluates a post-start source sentinel as raw f32");
    require_number(native_range, 3, "R", 0.0,
                   "RANGE evaluates later missing bounds instead of reopening the gate");
    require_number(native_range, 4, "R", 1.0,
                   "RANGE narrows value and bounds through raw f32 before tolerance checks");

    const auto native_literals = tdx::evaluate_formula_source_document(
        sample(3), "E:EXP(1);L:LN(0.5);G:LOG(1);", {},
        "NATIVEEXPLOGCONSTANTS");
    const double expected_exp_one = static_cast<double>(
        static_cast<float>(std::exp(1.0F)));
    const double expected_log_half = static_cast<double>(
        static_cast<float>(std::log(0.5F)));
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_literals, index, "E", expected_exp_one,
                       "EXP direct literal uses the type-3 f32 broadcast path");
        require_number(native_literals, index, "L", expected_log_half,
                       "LN direct literal uses the type-3 f32 broadcast path");
        require_number(native_literals, index, "G", 0.0,
                       "LOG direct literal broadcasts its f32 result");
    }

    auto logarithm_sample = sample(6);
    set_close_series(logarithm_sample, {0.5, 2.0, -1.0, 4.0, 5.0, 8.0});
    const auto native_logarithms = tdx::evaluate_formula_source_document(
        logarithm_sample,
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
        "L:LN(X);G:LOG(X);E:EXP(IF(CURRBARSCOUNT=5,89,X));",
        {}, "NATIVEEXPLOGSERIES");
    const double ln_two = static_cast<double>(
        static_cast<float>(std::log(2.0F)));
    const double ln_four = static_cast<double>(
        static_cast<float>(std::log(4.0F)));
    require_null(native_logarithms, 0, "L",
                 "LN Series path applies the native first-bar gate");
    require_number(native_logarithms, 1, "L", ln_two,
                   "LN starts at the first admissible Series value");
    require_number(native_logarithms, 2, "L", ln_two,
                   "LN carries previous output across a negative operand");
    require_number(native_logarithms, 3, "L", ln_four,
                   "LN stores its CRT result through raw f32");
    require_number(native_logarithms, 4, "L", ln_four,
                   "LN carries previous output across an internal sentinel");
    require_number(native_logarithms, 1, "E",
                   static_cast<double>(static_cast<float>(std::exp(0.5F))),
                   "EXP carries previous output across a value above 88");
    require_number(native_logarithms, 4, "G",
                   static_cast<double>(static_cast<float>(std::log10(4.0F))),
                   "LOG carries previous output across an internal sentinel");

    const auto native_log_series_constant =
        tdx::evaluate_formula_source_document(
            sample(3), "X:=CLOSE*0+0.5;L:LN(X);", {},
            "NATIVELOGSERIESCONSTANT");
    require_null(native_log_series_constant, 0, "L",
                 "bar-derived constant remains on the LN Series path");
    require_number(native_log_series_constant, 1, "L", expected_log_half,
                   "LN Series path resumes after its native first-bar gate");

    const auto native_exp_overflow = tdx::evaluate_formula_source_document(
        sample(2), "E:EXP(89);", {}, "NATIVEEXPOVERFLOW");
    require_null(native_exp_overflow, 0, "E",
                 "EXP type-3 path rejects output above its native range");
    require_null(native_exp_overflow, 1, "E",
                 "EXP rejected literal leaves the broadcast output missing");

    const auto native_trig_literals = tdx::evaluate_formula_source_document(
        sample(3), "A:ACOS(0.5);S:ASIN(0.5);T:TAN(0.5);", {},
        "NATIVETRIGLITERALS");
    const double acos_half = static_cast<double>(
        static_cast<float>(std::acos(0.5F)));
    const double asin_half = static_cast<double>(
        static_cast<float>(std::asin(0.5F)));
    const double tan_half = static_cast<double>(
        static_cast<float>(std::tan(0.5F)));
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_trig_literals, index, "A", acos_half,
                       "ACOS direct literal uses its raw-f32 type-3 path");
        require_number(native_trig_literals, index, "S", asin_half,
                       "ASIN direct literal uses its raw-f32 type-3 path");
        require_number(native_trig_literals, index, "T", tan_half,
                       "TAN direct literal uses its raw-f32 type-3 path");
    }

    const auto native_direct_trig_literals =
        tdx::evaluate_formula_source_document(
            sample(3),
            "A:ATAN(1.00000006);C:COS(16777217);S:SIN(16777217);",
            {}, "NATIVEDIRECTTRIGLITERALS");
    const float literal_atan_operand = static_cast<float>(1.00000006);
    const float literal_large_operand = static_cast<float>(16777217.0);
    const double atan_literal = static_cast<double>(static_cast<float>(
        std::atan(static_cast<double>(literal_atan_operand))));
    const double cos_literal = static_cast<double>(static_cast<float>(
        std::cos(static_cast<double>(literal_large_operand))));
    const double sin_literal = static_cast<double>(static_cast<float>(
        std::sin(static_cast<double>(literal_large_operand))));
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_direct_trig_literals, index, "A", atan_literal,
                       "ATAN direct literal narrows its operand through raw f32");
        require_number(native_direct_trig_literals, index, "C", cos_literal,
                       "COS direct literal narrows its operand through raw f32");
        require_number(native_direct_trig_literals, index, "S", sin_literal,
                       "SIN direct literal narrows its operand through raw f32");
    }

    auto direct_trig_sample = sample(4);
    set_close_series(direct_trig_sample,
                     {16777217.0, 0.3, 0.1, -12345.6789});
    const auto native_direct_trig_series =
        tdx::evaluate_formula_source_document(
            direct_trig_sample,
            "X:=IF(CURRBARSCOUNT=4,DRAWNULL,"
            "IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE));"
            "A:ATAN(X);C:COS(X);S:SIN(X);",
            {}, "NATIVEDIRECTTRIGSERIES");
    require_null(native_direct_trig_series, 0, "A",
                 "ATAN numeric Series preserves a missing point");
    require_number(native_direct_trig_series, 1, "A",
                   static_cast<double>(static_cast<float>(std::atan(
                       static_cast<double>(static_cast<float>(0.3))))),
                   "ATAN numeric Series computes from its raw-f32 operand");
    require_null(native_direct_trig_series, 2, "C",
                 "COS numeric Series preserves an internal missing point");
    require_number(native_direct_trig_series, 3, "C",
                   static_cast<double>(static_cast<float>(std::cos(
                       static_cast<double>(static_cast<float>(-12345.6789))))),
                   "COS numeric Series computes from its raw-f32 operand");
    require_number(native_direct_trig_series, 3, "S",
                   static_cast<double>(static_cast<float>(std::sin(
                       static_cast<double>(static_cast<float>(-12345.6789))))),
                   "SIN numeric Series computes from its raw-f32 operand");

    auto trig_sample = sample(6);
    set_close_series(trig_sample, {0.5, 0.25, 2.0, -0.5, 0.75, 1.0});
    const auto native_trig_series = tdx::evaluate_formula_source_document(
        trig_sample,
        "X:=IF(CURRBARSCOUNT=6,DRAWNULL,"
        "IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE));"
        "A:ACOS(X);S:ASIN(X);T:TAN(X);",
        {}, "NATIVETRIGSERIES");
    require_null(native_trig_series, 0, "A",
                 "ACOS skips the leading canonical sentinel");
    require_number(native_trig_series, 1, "A",
                   static_cast<double>(static_cast<float>(std::acos(0.25F))),
                   "ACOS computes through a raw-f32 operand and output");
    require_number(native_trig_series, 2, "A",
                   static_cast<double>(static_cast<float>(std::acos(0.25F))),
                   "ACOS carries its previous output across an invalid domain");
    require_number(native_trig_series, 4, "A",
                   static_cast<double>(static_cast<float>(std::acos(-0.5F))),
                   "ACOS carries across a post-start sentinel");
    require_number(native_trig_series, 2, "S",
                   static_cast<double>(static_cast<float>(std::asin(0.25F))),
                   "ASIN carries its previous output across an invalid domain");
    const double tangent_sentinel = static_cast<double>(static_cast<float>(
        std::tan(static_cast<double>(-4.0398103e34F))));
    require_number(native_trig_series, 4, "T", tangent_sentinel,
                   "TAN processes a post-start sentinel as its finite raw f32 value");

    const auto native_acos_literal_boundary =
        tdx::evaluate_formula_source_document(
            sample(2), "A:ACOS(1.000005);", {},
            "NATIVEACOSLITERALBOUNDARY");
    require_null(native_acos_literal_boundary, 0, "A",
                 "ACOS type-3 path uses the exact native [-1,1] domain");
    require_null(native_acos_literal_boundary, 1, "A",
                 "invalid ACOS literal leaves every broadcast point missing");

    const auto native_sign = tdx::evaluate_formula_source_document(
        sample(5),
        "P:SIGN(0.0000099999995);N:SGN(-0.0000099999995);"
        "Z:SIGN(1E-50);M:SIGN(DRAWNULL);",
        {}, "NATIVESIGN");
    for (std::size_t index = 0; index < 5; ++index) {
        require_number(native_sign, index, "P", 1.0,
                       "SIGN compares the rounded raw-f32 positive endpoint");
        require_number(native_sign, index, "N", -1.0,
                       "SGN shares the raw-f32 inclusive negative endpoint");
        require_number(native_sign, index, "Z", 0.0,
                       "SIGN treats a float-underflowed operand as exact zero");
        require_null(native_sign, index, "M",
                     "SIGN preserves the canonical missing sentinel");
    }

    auto fraction_sample = sample(5);
    set_close_series(fraction_sample,
                     {2147483648.0, -2147483648.0, 1.99995, -1.99995, 0.1});
    const auto native_fraction = tdx::evaluate_formula_source_document(
        fraction_sample, "F:FRACPART(CLOSE);", {}, "NATIVEFRACPART");
    require_number(native_fraction, 0, "F", 4294967296.0,
                   "FRACPART keeps native i32-indefinite behavior beyond the signed range");
    require_number(native_fraction, 1, "F", 0.0,
                   "FRACPART truncates a representable negative raw-f32 integer");
    require_number(native_fraction, 2, "F",
                   static_cast<double>(static_cast<float>(
                       static_cast<double>(static_cast<float>(1.99995)) - 2.0)),
                   "FRACPART applies the positive native adjustment");
    require_number(native_fraction, 3, "F",
                   static_cast<double>(static_cast<float>(
                       static_cast<double>(static_cast<float>(-1.99995)) + 2.0)),
                   "FRACPART applies the negative native adjustment");
    require_number(native_fraction, 4, "F",
                   static_cast<double>(static_cast<float>(0.1)),
                   "FRACPART writes its result through raw f32");

    auto fraction_missing_sample = sample(4);
    set_close_series(fraction_missing_sample, {0.25, 1.25, 2.25, 3.25});
    const auto native_fraction_missing =
        tdx::evaluate_formula_source_document(
            fraction_missing_sample,
            "X:=IF(CURRBARSCOUNT=4,DRAWNULL,"
            "IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE));F:FRACPART(X);",
            {}, "NATIVEFRACPARTMISSING");
    require_null(native_fraction_missing, 0, "F",
                 "FRACPART skips a leading canonical sentinel");
    require_null(native_fraction_missing, 2, "F",
                 "FRACPART maps a processed internal sentinel back to missing");
}

}  // namespace formula_engine_test
