#include "formula_engine_test_support.hpp"

namespace formula_engine_test {
namespace {

void set_close_series(tdx::Json& document,
                      const std::vector<double>& chronological_values) {
    auto& bars = document["bars"].as_array();
    require(bars.size() == chronological_values.size(),
            "native-operator fixture size");
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

void set_ohlc_series(
    tdx::Json& document,
    const std::vector<std::array<double, 3>>& chronological_values) {
    auto& bars = document["bars"].as_array();
    require(bars.size() == chronological_values.size(),
            "native-operator OHLC fixture size");
    for (std::size_t index = 0; index < chronological_values.size(); ++index) {
        auto& bar = bars[bars.size() - 1 - index];
        const auto& [high, low, close] = chronological_values[index];
        bar["open"] = close;
        bar["high"] = high;
        bar["low"] = low;
        bar["close"] = close;
        bar["amount"] = std::abs(close) * 10000.0;
    }
}

void require_null(const tdx::Json& result, std::size_t index,
                  const std::string& output, const std::string& message) {
    require(point_value(result, index, output).is_null(), message);
}

void require_number(const tdx::Json& result, std::size_t index,
                    const std::string& output, double expected,
                    double tolerance, const std::string& message) {
    const auto& value = point_value(result, index, output);
    require(value.is_number() &&
                std::abs(value.as_number() - expected) <= tolerance,
            message);
}

void require_zero_sign(const tdx::Json& result, std::size_t index,
                       const std::string& output, bool negative,
                       const std::string& message) {
    const auto& value = point_value(result, index, output);
    require(value.is_number() && value.as_number() == 0.0 &&
                std::signbit(value.as_number()) == negative,
            message);
}

float native_sma_step(float source, float previous, int period, int weight) {
    return static_cast<float>(
        (static_cast<double>(source) * static_cast<float>(weight) +
         static_cast<double>(previous) *
             static_cast<float>(period - weight)) /
        static_cast<float>(period));
}

}  // namespace

void run_native_operator_tests() {
    const auto native_if = tdx::evaluate_formula_source_document(
        sample(4),
        "COND:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,0,IF(CURRBARSCOUNT=2,1,DRAWNULL)));"
        "I:IF(COND,16777217,0.1);F:IFF(COND,16777217,0.1);"
        "N:IFN(COND,16777217,0.1);"
        "S:STRCMP(IF(COND,'Y','N'),'Y');",
        {}, "NATIVEIFFLOAT");
    require_null(native_if, 0, "I",
                 "IF preserves its leading condition sentinel");
    require_null(native_if, 0, "F",
                 "IFF preserves its leading condition sentinel");
    require_number(native_if, 1, "I",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IF writes its selected false branch through raw float");
    require_number(native_if, 1, "F",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IFF writes its selected false branch through raw float");
    require_number(native_if, 2, "I", 16777216.0, 0.0,
                   "IF narrows its selected true branch through raw float");
    require_number(native_if, 2, "F", 16777216.0, 0.0,
                   "IFF narrows its selected true branch through raw float");
    require_number(native_if, 3, "I", 16777216.0, 0.0,
                   "IF treats a post-start condition sentinel as native true");
    require_number(native_if, 3, "F", 16777216.0, 0.0,
                   "IFF treats a post-start condition sentinel as native true");
    require_number(native_if, 1, "N", 16777216.0, 0.0,
                   "IFN writes its selected false branch through raw float");
    require_number(native_if, 2, "N",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IFN writes its selected true branch through raw float");
    require_number(native_if, 3, "N",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IFN treats a post-start condition sentinel as native true");
    for (std::size_t index = 0; index < 4; ++index)
        require_number(native_if, index, "S", 1.0, 0.0,
                       "STRCMP compares final string handles once and broadcasts");

    const auto native_if_tiny = tdx::evaluate_formula_source_document(
        sample(1),
        "I:IF(1E-50,16777217,0.1);F:IFF(1E-50,16777217,0.1);"
        "N:IFN(1E-50,16777217,0.1);"
        "S:STRCMP(IF(1E-50,'Y','N'),'N');",
        {}, "NATIVEIFRAWZERO");
    require_number(native_if_tiny, 0, "I",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IF compares its condition after raw-f32 narrowing");
    require_number(native_if_tiny, 0, "F",
                   static_cast<double>(static_cast<float>(0.1)), 0.0,
                   "IFF compares its condition after raw-f32 narrowing");
    require_number(native_if_tiny, 0, "N", 16777216.0, 0.0,
                   "IFN keeps inverse selection after raw-f32 zero folding");
    require_number(native_if_tiny, 0, "S", 1.0, 0.0,
                   "string IF shares the raw-f32 exact-zero condition rule");

    auto refdate_sample = sample(5);
    set_close_series(refdate_sample,
                     {16777217.0, 20.0, 30.0, 40.0, 50.0});
    const auto native_refdate = tdx::evaluate_formula_source_document(
        refdate_sample,
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,CLOSE);"
        "L:REFDATE(X,1260101);M:REFDATE(CLOSE,DRAWNULL);"
        "E:REFDATE(CLOSE,1260100);F:REFDATE(CLOSE,1260101);"
        "D:REFDATE(CLOSE,1260104.99);",
        {}, "NATIVEREFDATE");
    for (std::size_t index = 0; index < 5; ++index) {
        require_null(native_refdate, index, "L",
                     "REFDATE broadcasts a selected leading source sentinel");
        require_number(native_refdate, index, "M", 50.0, 0.0,
                       "REFDATE maps a missing final target through native INT_MIN and unsigned comparison");
        require_null(native_refdate, index, "E",
                     "REFDATE leaves output missing when the target precedes every bar date");
        require_number(native_refdate, index, "F", 16777216.0, 0.0,
                       "REFDATE broadcasts the selected source through raw float");
        require_number(native_refdate, index, "D", 50.0, 0.0,
                       "REFDATE narrows 1260104.99 to the native 1260105 float before conversion");
    }

    auto included_sample = sample(5);
    set_ohlc_series(included_sample,
                    {{10.0, 0.0, 5.0},
                     {9.0, 1.0, 5.0},
                     {8.0, 2.0, 5.0},
                     {7.0, 3.0, 5.0},
                     {11.0, -1.0, 5.0}});
    const auto included = tdx::evaluate_formula_source_document(
        included_sample,
        "B:INCLUDED(0,0);F:INCLUDEDV(0,0);"
        "L:INCLUDED(IF(CURRBARSCOUNT=1,0,1),"
        "IF(CURRBARSCOUNT=1,0,1));",
        {}, "NATIVEINCLUDEDREADONLY");
    require_number(included, 0, "B", 0.0, 0.0,
                   "INCLUDED excludes the current bar from its backward search");
    require_number(included, 1, "B", 1.0, 0.0,
                   "INCLUDED marks a range contained by its prior bar");
    require_number(included, 2, "B", 1.0, 0.0,
                   "INCLUDED marks a second nested range");
    require_number(included, 3, "B", 1.0, 0.0,
                   "INCLUDED keeps its native included-candidate skip mask");
    require_number(included, 4, "B", 0.0, 0.0,
                   "INCLUDED rejects a range that expands beyond prior bars");
    require_number(included, 0, "F", 1.0, 0.0,
                   "INCLUDEDV detects containment by a future bar");
    require_number(included, 1, "F", 1.0, 0.0,
                   "INCLUDEDV scans forward using its result skip mask");
    require_number(included, 2, "F", 1.0, 0.0,
                   "INCLUDEDV detects a later containing range");
    require_number(included, 3, "F", 1.0, 0.0,
                   "INCLUDEDV retains its read-only lookahead result");
    require_number(included, 4, "F", 0.0, 0.0,
                   "INCLUDEDV has no candidate after the final bar");
    require_number(included, 1, "L", 1.0, 0.0,
                   "INCLUDED reads selector and limit only from the final bar");

    auto limit_included_sample = sample(4);
    set_ohlc_series(limit_included_sample,
                    {{10.0, 0.0, 5.0},
                     {12.0, -2.0, 5.0},
                     {11.0, -1.0, 5.0},
                     {9.0, 1.0, 5.0}});
    const auto limit_included = tdx::evaluate_formula_source_document(
        limit_included_sample,
        "B1:INCLUDED(0,1);B2:INCLUDED(0,2);",
        {}, "NATIVEINCLUDEDLIMIT");
    require_number(limit_included, 2, "B1", 1.0, 0.0,
                   "INCLUDED limit one still tests an immediate unmarked candidate");
    require_number(limit_included, 3, "B1", 0.0, 0.0,
                   "INCLUDED limit one stops after an immediate marked candidate");
    require_number(limit_included, 3, "B2", 1.0, 0.0,
                   "INCLUDED limit two reaches the next unmarked candidate");
    auto forward_limit_included_sample = sample(4);
    set_ohlc_series(forward_limit_included_sample,
                    {{9.0, 1.0, 5.0},
                     {11.0, -1.0, 5.0},
                     {12.0, -2.0, 5.0},
                     {10.0, 0.0, 5.0}});
    const auto forward_limit_included = tdx::evaluate_formula_source_document(
        forward_limit_included_sample,
        "F1:INCLUDEDV(0,1);F2:INCLUDEDV(0,2);",
        {}, "NATIVEINCLUDEDVLIMIT");
    require_number(forward_limit_included, 1, "F1", 1.0, 0.0,
                   "INCLUDEDV limit one tests an immediate unmarked candidate");
    require_number(forward_limit_included, 0, "F1", 0.0, 0.0,
                   "INCLUDEDV limit one stops after an immediate marked candidate");
    require_number(forward_limit_included, 0, "F2", 1.0, 0.0,
                   "INCLUDEDV limit two reaches the next unmarked candidate");

    auto body_included_sample = sample(2);
    set_ohlc_series(body_included_sample,
                    {{10.0, 0.0, 2.0},
                     {11.0, -1.0, 3.0}});
    auto& body_bars = body_included_sample["bars"].as_array();
    body_bars[body_bars.size() - 1]["open"] = 8.0;
    body_bars[body_bars.size() - 2]["open"] = 7.0;
    const auto body_included = tdx::evaluate_formula_source_document(
        body_included_sample,
        "R:INCLUDED(0,0);D:INCLUDED(1,0);X:INCLUDED(2,0);",
        {}, "NATIVEINCLUDEDBODY");
    require_number(body_included, 1, "R", 0.0, 0.0,
                   "INCLUDED selector zero rejects expanded high-low range");
    require_number(body_included, 1, "D", 1.0, 0.0,
                   "INCLUDED selector one compares open-close bodies");
    require_number(body_included, 1, "X", 0.0, 0.0,
                   "INCLUDED rejects selector values outside zero and one");

    auto tolerance_included_sample = sample(4);
    set_ohlc_series(tolerance_included_sample,
                    {{1.0, 0.0, 0.5},
                     {1.0000099, 0.0, 0.5},
                     {2.0, 0.0, 0.5},
                     {2.0000101, 0.0, 0.5}});
    const auto tolerance_included = tdx::evaluate_formula_source_document(
        tolerance_included_sample,
        "B:INCLUDED(0,1);F:INCLUDEDV(0,1);",
        {}, "NATIVEINCLUDEDTOLERANCE");
    require_number(tolerance_included, 1, "B", 1.0, 0.0,
                   "INCLUDED accepts expansion inside its absolute tolerance");
    require_number(tolerance_included, 3, "B", 0.0, 0.0,
                   "INCLUDED rejects expansion beyond its strict tolerance");
    require_number(tolerance_included, 2, "F", 1.0, 0.0,
                   "INCLUDEDV accepts containment by an expanded future range");
    require_number(tolerance_included, 0, "F", 1.0, 0.0,
                   "INCLUDEDV applies the same absolute tolerance forward");

    auto native_sar_sample = sample(6);
    set_close_series(native_sar_sample, {10.0, 9.0, 8.0, 7.0, 12.0, 13.0});
    const auto native_sar = tdx::evaluate_formula_source_document(
        native_sar_sample,
        "S:SAR(2,100,100);T:SARTURN(2,100,100);",
        {}, "NATIVESARSTATE");
    require_null(native_sar, 0, "S",
                 "SAR waits for its final-period seed bar");
    require_number(native_sar, 1, "S", 9.0, 0.0,
                   "SAR seeds from the first-period tolerant low");
    require_number(native_sar, 2, "S", 9.0, 0.0,
                   "SAR reverses from its fixed initial rising state");
    require_number(native_sar, 3, "S", 8.0, 0.0,
                   "SAR applies the falling prior-high boundary");
    require_number(native_sar, 4, "S", 7.0, 0.0,
                   "SAR reverses through its native prior-low boundary");
    require_number(native_sar, 5, "S", 12.0, 0.0,
                   "SAR resumes its rising prior-low boundary");
    require_null(native_sar, 0, "T",
                 "SARTURN preserves the leading SAR sentinel");
    require_number(native_sar, 1, "T", 9.0, 0.0,
                   "SARTURN retains SAR at its first valid bar");
    require_number(native_sar, 2, "T", -1.0, 0.0,
                   "SARTURN emits a downward close crossing");
    require_number(native_sar, 3, "T", 0.0, 0.0,
                   "SARTURN suppresses a repeated below state");
    require_number(native_sar, 4, "T", 1.0, 0.0,
                   "SARTURN emits an upward close crossing");
    require_number(native_sar, 5, "T", 0.0, 0.0,
                   "SARTURN suppresses a repeated above state");

    const auto final_parameter_sar =
        tdx::evaluate_formula_source_document(
            native_sar_sample,
            "N:=IF(CURRBARSCOUNT=1,2,4);"
            "A:=IF(CURRBARSCOUNT=1,100,0);"
            "S:SAR(N,A,A);I:SAR(6,100,100);J:SARTURN(6,100,100);",
            {}, "NATIVESARFINALPARAMETERS");
    require_number(final_parameter_sar, 1, "S", 9.0, 0.0,
                   "SAR reads N, step, and maximum only from the final bar");
    require_number(final_parameter_sar, 4, "S", 7.0, 0.0,
                   "SAR keeps final-bar parameters through both directions");
    require_null(final_parameter_sar, 5, "I",
                 "SAR rejects a period equal to the complete series size");
    require_null(final_parameter_sar, 5, "J",
                 "SARTURN retains missing when its nested SAR cannot seed");

    auto asymmetric_sar_sample = sample(3);
    set_ohlc_series(asymmetric_sar_sample,
                    {{100.0, 0.0, 50.0},
                     {10.0, 0.0, 5.0},
                     {10.0, -1.0, 0.0}});
    const auto asymmetric_sar = tdx::evaluate_formula_source_document(
        asymmetric_sar_sample, "S:SAR(2,20,20);", {},
        "NATIVESARASYMMETRICREVERSAL");
    const double asymmetric_projection = static_cast<double>(
        static_cast<float>(100.0 + (-1.0 - 100.0) *
                                      static_cast<double>(0.2F)));
    require_number(asymmetric_sar, 2, "S", asymmetric_projection, 0.0,
                   "SAR rising-to-falling reversal advances from the old extreme");

    auto float_sar_sample = sample(3);
    set_close_series(float_sar_sample,
                     {16777217.0, 16777218.0, 16777220.0});
    const auto float_sar = tdx::evaluate_formula_source_document(
        float_sar_sample, "S:SAR(1,100,100);", {}, "NATIVESARFLOAT");
    require_number(float_sar, 0, "S", 16777216.0, 0.0,
                   "SAR narrows its seed low through native float");

    auto near_sar_sample = sample(2);
    set_close_series(near_sar_sample, {1.0, 0.9999899});
    const auto near_sar = tdx::evaluate_formula_source_document(
        near_sar_sample, "S:SAR(1,100,100);", {}, "NATIVESARTOLERANCENEAR");
    require_number(near_sar, 1, "S",
                   static_cast<double>(0.9999899F), 0.0,
                   "SAR stays rising inside its relative-plus-absolute tolerance");

    auto far_sar_sample = sample(2);
    set_close_series(far_sar_sample, {1.0, 0.9999898});
    const auto far_sar = tdx::evaluate_formula_source_document(
        far_sar_sample, "S:SAR(1,100,100);", {}, "NATIVESARTOLERANCEFAR");
    require_number(far_sar, 1, "S", 1.0, 0.0,
                   "SAR reverses beyond its strict native tolerance boundary");

    auto native_scalar_sample = sample(6);
    set_close_series(native_scalar_sample,
                     {16777217.0, -0.0, 0.1, -0.1, 1.0, 2.0});
    const auto native_absolute = tdx::evaluate_formula_source_document(
        native_scalar_sample,
        "X:=IF(CURRBARSCOUNT=6 OR CURRBARSCOUNT=3,DRAWNULL,CLOSE);"
        "A:ABS(X);F:ABS(CLOSE);",
        {}, "NATIVEABS");
    require_number(native_absolute, 0, "F", 16777216.0, 0.0,
                   "ABS narrows the 16777217 discriminator through native float");
    require_null(native_absolute, 0, "A",
                 "ABS preserves its leading canonical sentinel");
    require_zero_sign(native_absolute, 1, "A", false,
                      "ABS clears the sign of native negative zero");
    require_number(native_absolute, 2, "A",
                   static_cast<double>(0.1F), 0.0,
                   "ABS stores a positive fractional input through float");
    require_null(native_absolute, 3, "A",
                 "ABS preserves an internal canonical sentinel after startup");
    require_number(native_absolute, 3, "F",
                   static_cast<double>(0.1F), 0.0,
                   "ABS applies native float fabs to a negative fraction");
    require_number(native_absolute, 4, "A", 1.0, 0.0,
                   "ABS resumes after an internal canonical sentinel");

    const auto overflow_absolute = tdx::evaluate_formula_source_document(
        sample(1), "H:ABS(1E100);M:ABS(DRAWNULL);", {},
        "NATIVEABSNONFINITE");
    require_null(overflow_absolute, 0, "H",
                 "ABS safely maps native float overflow to missing");
    require_null(overflow_absolute, 0, "M",
                 "ABS leaves an all-sentinel source missing");

    const auto native_modulo = tdx::evaluate_formula_source_document(
        sample(1),
        "P:MOD(5.4,2);N:MOD(-5.9,2);"
        "L:MOD(5.4,0.4969);H:MOD(5.4,0.497);"
        "F:MOD(16777217,20000000);"
        "A:MOD(DRAWNULL,2);B:MOD(5.4,DRAWNULL);"
        "U:MOD(-2147483648,-1.6);",
        {}, "NATIVEMOD");
    require_number(native_modulo, 0, "P", 1.0, 0.0,
                   "MOD uses biased integers for the 5.4 discriminator");
    require_number(native_modulo, 0, "N", -1.0, 0.0,
                   "MOD preserves the C++ signed-remainder direction");
    require_null(native_modulo, 0, "L",
                 "MOD maps a divisor below the biased integer boundary to missing");
    require_number(native_modulo, 0, "H", 0.0, 0.0,
                   "MOD accepts a divisor at the biased integer boundary");
    require_number(native_modulo, 0, "F", 16777216.0, 0.0,
                   "MOD narrows operands and its integer result through float");
    require_null(native_modulo, 0, "A",
                 "MOD rejects a canonical-sentinel dividend");
    require_null(native_modulo, 0, "B",
                 "MOD rejects a canonical-sentinel divisor");
    require_number(native_modulo, 0, "U", 0.0, 0.0,
                   "MOD widens the INT_MIN remainder edge to avoid signed overflow");

    auto native_sqrt_sample = sample(6);
    set_close_series(native_sqrt_sample,
                     {16777217.0, -4.0, -9.0, 16.0, 25.0, 36.0});
    const auto native_sqrt = tdx::evaluate_formula_source_document(
        native_sqrt_sample,
        "A:SQRT(CLOSE);"
        "X:=IF(CURRBARSCOUNT>=5 OR CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
        "S:SQRT(X);"
        "Y:=IF(CURRBARSCOUNT=6,-9,CLOSE);R:SQRT(Y);",
        {}, "NATIVESQRT");
    require_number(native_sqrt, 0, "A", 4096.0, 0.0,
                   "SQRT narrows the 16777217 discriminator before sqrt");
    require_number(native_sqrt, 1, "A", 4096.0, 0.0,
                   "SQRT carries its prior result across a negative operand");
    require_number(native_sqrt, 2, "A", 4096.0, 0.0,
                   "SQRT retains its carried float result until recovery");
    require_number(native_sqrt, 3, "A", 4.0, 0.0,
                   "SQRT resumes on a valid positive float operand");
    require_null(native_sqrt, 0, "S",
                 "SQRT preserves its first leading canonical sentinel");
    require_null(native_sqrt, 1, "S",
                 "SQRT scans the complete leading sentinel region");
    require_null(native_sqrt, 2, "S",
                 "SQRT carries the leading sentinel at a negative first value");
    require_number(native_sqrt, 3, "S", 4.0, 0.0,
                   "SQRT starts after a negative first native value");
    require_number(native_sqrt, 4, "S", 4.0, 0.0,
                   "SQRT carries across an internal canonical sentinel");
    require_number(native_sqrt, 5, "S", 6.0, 0.0,
                   "SQRT recovers after an internal canonical sentinel");
    require_null(native_sqrt, 0, "R",
                 "SQRT leaves an invalid index-zero source missing");
    require_null(native_sqrt, 1, "R",
                 "SQRT carries the index-zero missing result forward");
    require_number(native_sqrt, 3, "R", 4.0, 0.0,
                   "SQRT recovers after an invalid index-zero run");

    const auto sqrt_boundaries = tdx::evaluate_formula_source_document(
        sample(2),
        "X:=IF(CURRBARSCOUNT=2,4,-0.00002);C:SQRT(X);"
        "Y:=IF(CURRBARSCOUNT=2,4,-0.000005);N:SQRT(Y);"
        "Q:SQRT(250);Z:SQRT(-0.0);",
        {}, "NATIVESQRTBOUNDARY");
    require_number(sqrt_boundaries, 1, "C", 2.0, 0.0,
                   "SQRT carries below its native negative tolerance");
    require_null(sqrt_boundaries, 1, "N",
                 "SQRT preserves the non-finite result inside its tolerance");
    require_number(sqrt_boundaries, 0, "Q",
                   15.8113880157470703125, 0.0,
                   "SQRT stores the 250 constant through a float result");
    require_zero_sign(sqrt_boundaries, 0, "Z", true,
                      "SQRT preserves native negative zero");

    auto native_intpart_sample = sample(6);
    set_close_series(native_intpart_sample,
                     {1.99995, -1.99995, 1.9998, -1.9998,
                      16777217.0, 2.0});
    const auto native_intpart = tdx::evaluate_formula_source_document(
        native_intpart_sample,
        "X:=IF(CURRBARSCOUNT=6 OR CURRBARSCOUNT=3,DRAWNULL,CLOSE);"
        "I:INTPART(X);F:INTPART(CLOSE);",
        {}, "NATIVEINTPART");
    require_number(native_intpart, 0, "F", 2.0, 0.0,
                   "INTPART adjusts 1.99995 across its positive integer edge");
    require_null(native_intpart, 0, "I",
                 "INTPART preserves its leading canonical sentinel");
    require_number(native_intpart, 1, "I", -2.0, 0.0,
                   "INTPART adjusts -1.99995 across its negative integer edge");
    require_number(native_intpart, 2, "I", 1.0, 0.0,
                   "INTPART leaves 1.9998 below its adjustment edge");
    require_number(native_intpart, 3, "I", -2147483648.0, 0.0,
                   "INTPART converts an internal sentinel after startup");
    require_number(native_intpart, 3, "F", -1.0, 0.0,
                   "INTPART leaves -1.9998 above its negative adjustment edge");
    require_number(native_intpart, 4, "I", 16777216.0, 0.0,
                   "INTPART narrows the 16777217 discriminator through float");

    const auto intpart_boundaries = tdx::evaluate_formula_source_document(
        sample(1), "H:INTPART(2147483648);M:INTPART(DRAWNULL);", {},
        "NATIVEINTPARTBOUNDARY");
    require_number(intpart_boundaries, 0, "H", -2147483648.0, 0.0,
                   "INTPART maps out-of-range conversion to native INT_MIN");
    require_null(intpart_boundaries, 0, "M",
                 "INTPART leaves an all-sentinel source missing");

    auto moving_average_sample = sample(4);
    set_close_series(moving_average_sample, {10.0, 20.0, 30.0, 40.0});
    const auto moving_average = tdx::evaluate_formula_source_document(
        moving_average_sample,
        "N0:MA(CLOSE,0);"
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);M:MA(X,3);"
        "Y:=IF(CURRBARSCOUNT=4,DRAWNULL,CLOSE);F:MA(Y,3);"
        "P:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,0,IF(CURRBARSCOUNT=2,-2,2)));D:MA(CLOSE,P);",
        {}, "NATIVEMA");
    for (std::size_t index = 0; index < 4; ++index)
        require_null(moving_average, index, "N0",
                     "MA with zero period remains native missing");
    require_null(moving_average, 0, "M", "MA waits for a complete window");
    require_null(moving_average, 1, "M", "MA waits for a complete window");
    require_number(moving_average, 2, "M", 10.0, 0.0,
                   "MA skips an internal missing value but divides by N");
    require_number(moving_average, 3, "M", 20.0, 0.0,
                   "MA keeps the fixed divisor after an internal missing value");
    require_null(moving_average, 0, "F", "MA preserves leading source missing");
    require_null(moving_average, 1, "F", "MA starts at the first valid source");
    require_null(moving_average, 2, "F",
                 "MA rejects a window crossing the first valid source");
    require_number(moving_average, 3, "F", 30.0, 0.0,
                   "MA accepts the first window fully inside valid history");
    require_null(moving_average, 0, "D", "MA rejects a missing period");
    require_null(moving_average, 1, "D", "MA rejects period zero");
    require_null(moving_average, 2, "D", "MA rejects a negative period");
    require_number(moving_average, 3, "D", 35.0, 0.0,
                   "MA accepts a later valid dynamic period");

    auto float_average_sample = sample(3);
    set_close_series(float_average_sample,
                     {16777216.0, 1.0, -16777216.0});
    const auto float_average = tdx::evaluate_formula_source_document(
        float_average_sample, "M:MA(CLOSE,3);", {}, "NATIVEFLOATMA");
    require_null(float_average, 0, "M", "float MA needs full history");
    require_null(float_average, 1, "M", "float MA needs full history");
    require_number(float_average, 2, "M",
                   static_cast<double>(static_cast<float>(1.0F / 3.0F)),
                   0.0, "MA accumulates backwards and narrows through float");

    auto native_period_average_sample = sample(3);
    set_close_series(native_period_average_sample, {10.0, 20.0, 30.0});
    const auto native_period_average = tdx::evaluate_formula_source_document(
        native_period_average_sample,
        "P:=2.99999999;M:MA(CLOSE,P);"
        "X:=IF(CURRBARSCOUNT=2,-4.0398103E34,CLOSE);S:MA(X,2);"
        "Y:=IF(CURRBARSCOUNT=3,-4.0398103E34,CLOSE);L:MA(Y,2);",
        {}, "NATIVEMAPERIOD");
    require_null(native_period_average, 0, "M",
                 "MA narrows its period to float before truncation");
    require_null(native_period_average, 1, "M",
                 "MA uses the native float-rounded three-bar period");
    require_number(native_period_average, 2, "M", 20.0, 0.0,
                   "MA accepts the first complete float-rounded window");
    require_null(native_period_average, 0, "S",
                 "MA still waits for its first complete two-bar window");
    require_number(native_period_average, 1, "S", 5.0, 0.0,
                   "MA skips an internal canonical sentinel but divides by N");
    require_number(native_period_average, 2, "S", 15.0, 0.0,
                   "MA skips a prior canonical sentinel in the moving window");
    require_null(native_period_average, 0, "L",
                 "MA treats a leading canonical sentinel as missing");
    require_null(native_period_average, 1, "L",
                 "MA rejects history crossing its first native source");
    require_number(native_period_average, 2, "L", 25.0, 0.0,
                   "MA starts windows relative to the first native source");

    auto dynamic_sum_sample = sample(5);
    set_close_series(dynamic_sum_sample, {10.0, 20.0, 30.0, 40.0, 50.0});
    const auto dynamic_sum = tdx::evaluate_formula_source_document(
        dynamic_sum_sample,
        "P:=IF(CURRBARSCOUNT=5,3,IF(CURRBARSCOUNT=4,0,"
        "IF(CURRBARSCOUNT=3,-2,2)));S:SUM(CLOSE,P);",
        {}, "NATIVESUMDYNAMIC");
    const double expected_dynamic_sum[]{10.0, 30.0, 60.0, 70.0, 90.0};
    for (std::size_t index = 0; index < 5; ++index)
        require_number(dynamic_sum, index, "S", expected_dynamic_sum[index],
                       0.0,
                       "SUM retains its prefix until a dynamic window is valid");

    auto missing_sum_sample = sample(4);
    set_close_series(missing_sum_sample, {10.0, 20.0, 30.0, 40.0});
    const auto missing_sum = tdx::evaluate_formula_source_document(
        missing_sum_sample,
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);S:SUM(X,2);",
        {}, "NATIVESUMMISSING");
    const double expected_missing_sum[]{10.0, 10.0, 30.0, 70.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(missing_sum, index, "S", expected_missing_sum[index],
                       0.0,
                       "SUM carries its prefix and skips missing window values");

    auto float_sum_sample = sample(4);
    set_close_series(float_sum_sample,
                     {0.0, 16777216.0, 1.0, -16777216.0});
    const auto float_sum = tdx::evaluate_formula_source_document(
        float_sum_sample,
        "X:=IF(CURRBARSCOUNT=4,DRAWNULL,CLOSE);"
        "P:SUM(X,0);W:SUM(X,3);",
        {}, "NATIVEFLOATSUM");
    require_null(float_sum, 0, "P", "SUM preserves its leading missing region");
    require_number(float_sum, 2, "P", 16777216.0, 0.0,
                   "SUM narrows each prefix addition through float");
    require_number(float_sum, 3, "P", 0.0, 0.0,
                   "SUM zero period retains the native float prefix");
    require_number(float_sum, 3, "W", 1.0, 0.0,
                   "SUM recomputes a valid window from current to past in float");

    auto sum_bars_sample = sample(3);
    set_close_series(sum_bars_sample, {10.0, 10.0, 10.0});
    const auto native_sum_bars = tdx::evaluate_formula_source_document(
        sum_bars_sample, "S:SUMBARS(CLOSE,10);", {}, "NATIVESUMBARS");
    require_number(native_sum_bars, 0, "S", 0.0, 0.0,
                   "SUMBARS caps its first output at the left boundary");
    require_number(native_sum_bars, 1, "S", 1.0, 0.0,
                   "SUMBARS counts the current bar after its first output");
    require_number(native_sum_bars, 2, "S", 1.0, 0.0,
                   "SUMBARS preserves the native current-only count");

    const auto missing_sum_bars = tdx::evaluate_formula_source_document(
        sample(4),
        "X:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=1,DRAWNULL,1));"
        "A:=IF(CURRBARSCOUNT=2,DRAWNULL,2);S:SUMBARS(X,A);",
        {}, "NATIVESUMBARSMISSING");
    require_null(missing_sum_bars, 0, "S",
                 "SUMBARS preserves only its leading source-missing region");
    require_number(missing_sum_bars, 1, "S", 0.0, 0.0,
                   "SUMBARS applies its left-boundary fallback");
    require_number(missing_sum_bars, 2, "S", 0.0, 0.0,
                   "SUMBARS maps a later missing target through the native sentinel");
    require_number(missing_sum_bars, 3, "S", 2.0, 0.0,
                   "SUMBARS includes a later missing source in its native scan");

    auto stddev_sample = sample(4);
    set_close_series(stddev_sample,
                     {9.9999999e-6, 2.0e-5, 4.0e-5, 8.0e-5});
    const auto native_stddev = tdx::evaluate_formula_source_document(
        stddev_sample, "V:STDDEV(CLOSE,3);", {}, "NATIVESTDDEVFLOAT");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(native_stddev, index, "V",
                     "STDDEV waits one full period beyond its first source");
    require_number(native_stddev, 3, "V", 0.3465735912322998, 0.0,
                   "STDDEV narrows operands and every accumulation step to float");

    auto dynamic_extreme_sample = sample(4);
    set_close_series(dynamic_extreme_sample, {4.0, 2.0, 5.0, 3.0});
    const auto dynamic_extremes = tdx::evaluate_formula_source_document(
        dynamic_extreme_sample,
        "P:=IF(CURRBARSCOUNT=4,5,IF(CURRBARSCOUNT=3,0,"
        "IF(CURRBARSCOUNT=2,-2,2)));H:HHV(CLOSE,P);L:LLV(CLOSE,P);"
        "Q:=IF(CURRBARSCOUNT=3,DRAWNULL,P);"
        "HM:HHV(CLOSE,Q);LM:LLV(CLOSE,Q);",
        {}, "NATIVEEXTREMEDYNAMIC");
    const double expected_highest[]{4.0, 4.0, 5.0, 5.0};
    const double expected_lowest[]{4.0, 2.0, 2.0, 3.0};
    for (std::size_t index = 0; index < 4; ++index) {
        require_number(dynamic_extremes, index, "H", expected_highest[index],
                       0.0,
                       "HHV clamps invalid dynamic periods to available history");
        require_number(dynamic_extremes, index, "L", expected_lowest[index],
                       0.0,
                       "LLV clamps invalid dynamic periods to available history");
    }
    require_null(dynamic_extremes, 1, "HM",
                 "HHV preserves a missing dynamic period");
    require_null(dynamic_extremes, 1, "LM",
                 "LLV preserves a missing dynamic period");

    const auto missing_extremes = tdx::evaluate_formula_source_document(
        sample(3),
        "A:=IF(CURRBARSCOUNT=2,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,5,4));HA:HHV(A,3);LA:LLV(A,3);"
        "B:=IF(CURRBARSCOUNT=1,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,5,4));HB:HHV(B,3);LB:LLV(B,3);"
        "C:=IF(CURRBARSCOUNT=3,5,DRAWNULL);"
        "HC:HHV(C,2);LC:LLV(C,2);SC:SUM(C,2);",
        {}, "NATIVEEXTREMEMISSING");
    require_number(missing_extremes, 2, "HA", 5.0, 0.0,
                   "HHV ignores an internal missing candidate");
    require_number(missing_extremes, 2, "LA", 4.0, 0.0,
                   "LLV recovers when a finite candidate follows missing");
    require_number(missing_extremes, 2, "HB", 5.0, 0.0,
                   "HHV ignores a trailing missing candidate");
    require_null(missing_extremes, 2, "LB",
                 "LLV selects the native missing sentinel at a window tail");
    require_null(missing_extremes, 2, "HC",
                 "HHV leaves an all-missing window missing");
    require_null(missing_extremes, 2, "LC",
                 "LLV leaves an all-missing window missing");
    require_number(missing_extremes, 2, "SC", 0.0, 0.0,
                   "SUM emits zero for a valid all-missing rolling window");

    const auto tolerant_extremes = tdx::evaluate_formula_source_document(
        sample(2),
        "A:=IF(CURRBARSCOUNT=2,100,99.99999);H:HHV(A,2);"
        "B:=IF(CURRBARSCOUNT=2,100,100.00001);L:LLV(B,2);",
        {}, "NATIVEEXTREMETOLERANCE");
    require_number(tolerant_extremes, 1, "H",
                   static_cast<double>(static_cast<float>(99.99999)), 0.0,
                   "HHV selects the later candidate inside native tolerance");
    require_number(tolerant_extremes, 1, "L",
                   static_cast<double>(static_cast<float>(100.00001)), 0.0,
                   "LLV selects the later candidate inside native tolerance");

    auto cross_band_sample = sample(7);
    set_close_series(cross_band_sample,
                     {101.0, 100.0, 99.99997, 99.99996,
                      100.0, 100.00003, 99.99997});
    const auto cross_band = tdx::evaluate_formula_source_document(
        cross_band_sample, "X:CROSS(100,CLOSE);", {}, "NATIVECROSS");
    const double expected_cross[]{0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 7; ++index)
        require_number(cross_band, index, "X", expected_cross[index], 0.0,
                       "CROSS retains and resets its tolerant below-state");

    auto leading_cross_sample = sample(4);
    set_close_series(leading_cross_sample, {101.0, 101.0, 101.0, 99.99997});
    const auto leading_cross = tdx::evaluate_formula_source_document(
        leading_cross_sample,
        "A:=IF(CURRBARSCOUNT=4,DRAWNULL,100);"
        "B:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);X:CROSS(A,B);",
        {}, "NATIVECROSSLEADING");
    require_null(leading_cross, 0, "X",
                 "CROSS preserves leading missing operands");
    require_null(leading_cross, 1, "X",
                 "CROSS waits for both operands to become valid");
    require_number(leading_cross, 2, "X", 0.0, 0.0,
                   "CROSS initializes without firing on its first valid bar");
    require_number(leading_cross, 3, "X", 1.0, 0.0,
                   "CROSS fires after a strict below-state and tolerance band");

    auto later_missing_cross_sample = sample(4);
    set_close_series(later_missing_cross_sample, {101.0, 100.0, 101.0, 99.99997});
    const auto later_missing_cross = tdx::evaluate_formula_source_document(
        later_missing_cross_sample,
        "A:=IF(CURRBARSCOUNT=3,DRAWNULL,100);"
        "B:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);X:CROSS(A,B);",
        {}, "NATIVECROSSMISSING");
    const double expected_later_missing[]{0.0, 0.0, 1.0, 0.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(later_missing_cross, index, "X",
                       expected_later_missing[index], 0.0,
                       "CROSS uses the finite native sentinel after startup");

    auto float_arithmetic_sample = sample(2);
    set_close_series(float_arithmetic_sample, {16777217.0, 20.0});
    const auto float_arithmetic = tdx::evaluate_formula_source_document(
        float_arithmetic_sample,
        "S:CLOSE-16777216;M:CLOSE*1;"
        "SN:CLOSE-IF(CURRBARSCOUNT=2,DRAWNULL,1);"
        "MN:CLOSE*IF(CURRBARSCOUNT=2,DRAWNULL,1);",
        {}, "NATIVEFLOATARITHMETIC");
    require_number(float_arithmetic, 0, "S", 0.0, 0.0,
                   "binary subtraction narrows both operands through float");
    require_number(float_arithmetic, 0, "M", 16777216.0, 0.0,
                   "binary multiplication narrows both operands through float");
    require_number(float_arithmetic, 1, "S", -16777196.0, 0.0,
                   "binary subtraction stores a float result");
    require_number(float_arithmetic, 1, "M", 20.0, 0.0,
                   "binary multiplication stores a float result");
    require_null(float_arithmetic, 0, "SN",
                 "binary subtraction propagates a missing operand");
    require_null(float_arithmetic, 0, "MN",
                 "binary multiplication propagates a missing operand");

    auto exponential_sample = sample(4);
    set_close_series(exponential_sample, {10.0, 20.0, 30.0, 40.0});
    const auto native_exponential = tdx::evaluate_formula_source_document(
        exponential_sample,
        "N:=IF(CURRBARSCOUNT>=3,3,5);E:EMA(CLOSE,N);"
        "P:EXPMA(CLOSE,N);"
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);M:EMA(X,N);",
        {}, "NATIVEEMA");
    const double expected_ema[]{10.0, 15.0, 20.0,
        static_cast<double>(static_cast<float>(160.0 / 6.0))};
    const double expected_missing_ema[]{10.0, 15.0, 15.0,
        static_cast<double>(static_cast<float>(140.0 / 6.0))};
    for (std::size_t index = 0; index < 4; ++index) {
        require_number(native_exponential, index, "E", expected_ema[index], 0.0,
                       "EMA changes period without reseeding");
        require_number(native_exponential, index, "P", expected_ema[index], 0.0,
                       "EXPMA shares the native EMA handler");
        require_number(native_exponential, index, "M",
                       expected_missing_ema[index], 0.0,
                       "EMA carries its prior output across a missing source bar");
    }

    auto seeded_exponential_boundary_sample = sample(3);
    set_close_series(seeded_exponential_boundary_sample,
                     {10.0, 20.0, 30.0});
    const auto seeded_exponential_boundary =
        tdx::evaluate_formula_source_document(
            seeded_exponential_boundary_sample,
            "N:=IF(CURRBARSCOUNT=1,3.9,1);"
            "E:EXPMEMA(CLOSE,N);M:MEMA(CLOSE,N);",
            {}, "NATIVESEEDEDEXPONENTIALBOUNDARY");
    for (std::size_t index = 0; index < 2; ++index) {
        require_null(seeded_exponential_boundary, index, "E",
                     "EXPMEMA waits for its complete seed window");
    }
    require_number(seeded_exponential_boundary, 2, "E", 20.0, 0.0,
                   "EXPMEMA permits its seed on the final bar");
    for (std::size_t index = 0; index < 3; ++index) {
        require_null(seeded_exponential_boundary, index, "M",
                     "MEMA requires a bar after its seed window");
    }

    auto seeded_exponential_missing_sample = sample(6);
    set_close_series(seeded_exponential_missing_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0, 60.0});
    const auto seeded_exponential_missing =
        tdx::evaluate_formula_source_document(
            seeded_exponential_missing_sample,
            "X:=IF(CURRBARSCOUNT=6 OR CURRBARSCOUNT=4 OR "
            "CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
            "E:EXPMEMA(X,3);M:MEMA(X,3);",
            {}, "NATIVESEEDEDEXPONENTIALMISSING");
    for (const auto* output : {"E", "M"}) {
        for (std::size_t index = 0; index < 3; ++index) {
            require_null(seeded_exponential_missing, index, output,
                         "seeded exponential preserves its leading output region");
        }
        require_number(seeded_exponential_missing, 3, output,
                       26.6666660308837890625, 0.0,
                       "seeded exponential forward-fills an internal seed sentinel");
        require_number(seeded_exponential_missing, 4, output,
                       26.6666660308837890625, 0.0,
                       "seeded exponential carries across a post-seed sentinel");
    }
    require_number(seeded_exponential_missing, 5, "E",
                   43.333332061767578125, 0.0,
                   "EXPMEMA applies its native two-source recurrence");
    require_number(seeded_exponential_missing, 5, "M",
                   37.77777862548828125, 0.0,
                   "MEMA applies its native one-source recurrence");

    auto seeded_exponential_float_sample = sample(4);
    set_close_series(seeded_exponential_float_sample,
                     {16777216.0, 1.0, -16777216.0, 3.0});
    const auto seeded_exponential_float =
        tdx::evaluate_formula_source_document(
            seeded_exponential_float_sample,
            "E:EXPMEMA(CLOSE,3);M:MEMA(CLOSE,3);",
            {}, "NATIVESEEDEDEXPONENTIALFLOAT");
    for (const auto* output : {"E", "M"}) {
        require_number(seeded_exponential_float, 2, output, 0.0, 0.0,
                       "seeded exponential accumulates its seed through float");
    }
    require_number(seeded_exponential_float, 3, "E", 1.5, 0.0,
                   "EXPMEMA stores its recurrence through float");
    require_number(seeded_exponential_float, 3, "M", 1.0, 0.0,
                   "MEMA stores its recurrence through float");

    const auto invalid_seeded_exponential =
        tdx::evaluate_formula_source_document(
            sample(2),
            "N:=IF(CURRBARSCOUNT=1,DRAWNULL,1);"
            "E:EXPMEMA(CLOSE,N);M:MEMA(CLOSE,N);",
            {}, "NATIVESEEDEDEXPONENTIALINVALIDPERIOD");
    for (const auto* output : {"E", "M"}) {
        for (std::size_t index = 0; index < 2; ++index) {
            require_null(invalid_seeded_exponential, index, output,
                         "seeded exponential rejects a final sentinel period");
        }
    }

    auto smoothed_sample = sample(4);
    set_close_series(smoothed_sample, {10.0, 20.0, 30.0, 40.0});
    const auto smoothed = tdx::evaluate_formula_source_document(
        smoothed_sample,
        "N:=IF(CURRBARSCOUNT=1,3,100);"
        "M:=IF(CURRBARSCOUNT=1,1,99);S:SMA(CLOSE,N,M);",
        {}, "NATIVESMAFIXEDPARAMETERS");
    float smoothed_state = 10.0F;
    require_number(smoothed, 0, "S", smoothed_state, 0.0,
                   "SMA seeds from the first valid native float source");
    for (std::size_t index = 1; index < 4; ++index) {
        smoothed_state = native_sma_step(
            static_cast<float>((index + 1) * 10.0), smoothed_state, 3, 1);
        require_number(smoothed, index, "S", smoothed_state, 0.0,
                       "SMA uses final-bar N and M for the complete series");
    }

    const auto gated_smoothed = tdx::evaluate_formula_source_document(
        smoothed_sample,
        "EQ:SMA(CLOSE,3,3);LT:SMA(CLOSE,2,3);"
        "ZERO:SMA(CLOSE,0,-1);"
        "N:=IF(CURRBARSCOUNT=1,3,1);"
        "M:=IF(CURRBARSCOUNT=1,3,0);LAST:SMA(CLOSE,N,M);"
        "NEG:SMA(CLOSE,2,-1);TRUNC:SMA(CLOSE,3.9,1.9);",
        {}, "NATIVESMAGATE");
    for (std::size_t index = 0; index < 4; ++index) {
        require_null(gated_smoothed, index, "EQ",
                     "SMA rejects N equal to M");
        require_null(gated_smoothed, index, "LT",
                     "SMA rejects N below M");
        require_null(gated_smoothed, index, "ZERO",
                     "SMA rejects N below one even when N exceeds M");
        require_null(gated_smoothed, index, "LAST",
                     "SMA applies its final-bar gate to the complete series");
    }
    float negative_weight_state = 10.0F;
    float truncated_state = 10.0F;
    require_number(gated_smoothed, 0, "NEG", negative_weight_state, 0.0,
                   "SMA permits a negative M when N exceeds it");
    require_number(gated_smoothed, 0, "TRUNC", truncated_state, 0.0,
                   "SMA truncates its final native float parameters");
    for (std::size_t index = 1; index < 4; ++index) {
        const float source = static_cast<float>((index + 1) * 10.0);
        negative_weight_state =
            native_sma_step(source, negative_weight_state, 2, -1);
        truncated_state = native_sma_step(source, truncated_state, 3, 1);
        require_number(gated_smoothed, index, "NEG",
                       negative_weight_state, 0.0,
                       "SMA does not clamp a valid negative M");
        require_number(gated_smoothed, index, "TRUNC", truncated_state, 0.0,
                       "SMA stores each recurrence result as float");
    }

    auto leading_smoothed_sample = sample(4);
    set_close_series(leading_smoothed_sample,
                     {0.0, 16777217.0, 1.0, -16777216.0});
    const auto leading_smoothed = tdx::evaluate_formula_source_document(
        leading_smoothed_sample,
        "X:=IF(CURRBARSCOUNT=4,DRAWNULL,CLOSE);S:SMA(X,3,1);",
        {}, "NATIVESMALEADING");
    require_null(leading_smoothed, 0, "S",
                 "SMA preserves its leading missing source region");
    float leading_state = static_cast<float>(16777217.0);
    require_number(leading_smoothed, 1, "S", leading_state, 0.0,
                   "SMA narrows its first valid seed to float");
    leading_state = native_sma_step(1.0F, leading_state, 3, 1);
    require_number(leading_smoothed, 2, "S", leading_state, 0.0,
                   "SMA recurrence reads the float seed");
    leading_state = native_sma_step(-16777216.0F, leading_state, 3, 1);
    require_number(leading_smoothed, 3, "S", leading_state, 0.0,
                   "SMA carries a float-stored state between bars");

    auto sentinel_smoothed_sample = sample(3);
    set_close_series(sentinel_smoothed_sample, {10.0, 20.0, 30.0});
    const auto sentinel_smoothed = tdx::evaluate_formula_source_document(
        sentinel_smoothed_sample,
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);S:SMA(X,3,1);",
        {}, "NATIVESMASENTINEL");
    constexpr float tcalc_missing_sentinel = -4.0398103e34F;
    float sentinel_state = 10.0F;
    require_number(sentinel_smoothed, 0, "S", sentinel_state, 0.0,
                   "SMA starts at the first valid source");
    sentinel_state = native_sma_step(
        tcalc_missing_sentinel, sentinel_state, 3, 1);
    require_number(sentinel_smoothed, 1, "S", sentinel_state, 0.0,
                   "SMA consumes an internal native missing sentinel");
    sentinel_state = native_sma_step(30.0F, sentinel_state, 3, 1);
    require_number(sentinel_smoothed, 2, "S", sentinel_state, 0.0,
                   "SMA keeps the sentinel-derived float state afterward");

    auto mapped_sentinel_smoothed_sample = sample(26);
    set_close_series(mapped_sentinel_smoothed_sample,
                     std::vector<double>(26, 10.0));
    const auto mapped_sentinel_smoothed =
        tdx::evaluate_formula_source_document(
            mapped_sentinel_smoothed_sample,
            "X:=IF(CURRBARSCOUNT=26,CLOSE,DRAWNULL);S:SMA(X,2,1);",
            {}, "NATIVESMAMAPPEDSENTINEL");
    float mapped_sentinel_state = 10.0F;
    for (std::size_t index = 1; index < 25; ++index)
        mapped_sentinel_state = native_sma_step(
            tcalc_missing_sentinel, mapped_sentinel_state, 2, 1);
    require(mapped_sentinel_state != tcalc_missing_sentinel,
            "SMA mapped-sentinel fixture has a finite predecessor");
    require_number(mapped_sentinel_smoothed, 24, "S", mapped_sentinel_state,
                   0.0,
                   "SMA exposes a recurrence result before it reaches the sentinel");
    mapped_sentinel_state = native_sma_step(
        tcalc_missing_sentinel, mapped_sentinel_state, 2, 1);
    require(mapped_sentinel_state == tcalc_missing_sentinel,
            "SMA mapped-sentinel fixture reaches the exact native sentinel");
    require_null(mapped_sentinel_smoothed, 25, "S",
                 "SMA maps an exact native sentinel recurrence result to missing");

    auto leading_dma_sample = sample(5);
    set_close_series(leading_dma_sample, {10.0, 20.0, 30.0, 40.0, 50.0});
    const auto leading_dma = tdx::evaluate_formula_source_document(
        leading_dma_sample,
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,CLOSE);"
        "A:=IF(CURRBARSCOUNT=4,DRAWNULL,0.5);D:DMA(X,A);",
        {}, "NATIVEDMALEADING");
    require_null(leading_dma, 0, "D",
                 "DMA waits across a leading missing X operand");
    require_null(leading_dma, 1, "D",
                 "DMA waits across a leading missing A operand");
    require_number(leading_dma, 2, "D", 30.0, 0.0,
                   "DMA seeds from the first jointly valid native float X");
    require_number(leading_dma, 3, "D", 35.0, 0.0,
                   "DMA applies its ordinary half-weight recurrence");
    require_number(leading_dma, 4, "D", 42.5, 0.0,
                   "DMA retains its float state between ordinary bars");

    auto negative_dma_sample = sample(2);
    set_close_series(negative_dma_sample, {10.0, 20.0});
    const auto negative_dma = tdx::evaluate_formula_source_document(
        negative_dma_sample,
        "A:=IF(CURRBARSCOUNT=2,0.5,-0.5);D:DMA(CLOSE,A);",
        {}, "NATIVEDMANEGATIVE");
    require_number(negative_dma, 0, "D", 10.0, 0.0,
                   "DMA stores its first source through a float landing point");
    require_number(negative_dma, 1, "D", 5.0, 0.0,
                   "DMA extrapolates with a negative A instead of clamping it");

    auto boundary_dma_sample = sample(3);
    set_close_series(boundary_dma_sample, {10.0, 100.0, 200.0});
    const auto boundary_dma = tdx::evaluate_formula_source_document(
        boundary_dma_sample,
        "A:=IF(CURRBARSCOUNT=3,0.5,"
        "IF(CURRBARSCOUNT=2,0.9999898,0.9999899));D:DMA(CLOSE,A);",
        {}, "NATIVEDMABOUNDARY");
    constexpr float recurrent_alpha = 0.9999898F;
    const float recurrent_boundary = static_cast<float>(
        10.0 * (1.0 - static_cast<double>(recurrent_alpha)) +
        100.0 * static_cast<double>(recurrent_alpha));
    require_number(boundary_dma, 1, "D", recurrent_boundary, 0.0,
                   "DMA keeps the recurrence below its tolerant upper boundary");
    require_number(boundary_dma, 2, "D", 200.0, 0.0,
                   "DMA returns X directly above its strict tolerant boundary");

    auto float_dma_sample = sample(2);
    set_close_series(float_dma_sample, {16777217.0, 1.0});
    const auto float_dma = tdx::evaluate_formula_source_document(
        float_dma_sample, "D:DMA(CLOSE,0.5);", {}, "NATIVEDMAFLOAT");
    require_number(float_dma, 0, "D", 16777216.0, 0.0,
                   "DMA narrows its first X operand to native float");
    require_number(float_dma, 1, "D", 8388608.0, 0.0,
                   "DMA recurrence reads float X and prior float state");

    auto sentinel_dma_sample = sample(6);
    set_close_series(sentinel_dma_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0, 60.0});
    const auto sentinel_dma = tdx::evaluate_formula_source_document(
        sentinel_dma_sample,
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,CLOSE);"
        "A:=IF(CURRBARSCOUNT=3,DRAWNULL,"
        "IF(CURRBARSCOUNT=5 OR CURRBARSCOUNT=2,1.1,0.5));"
        "D:DMA(X,A);",
        {}, "NATIVEDMASENTINEL");
    require_number(sentinel_dma, 0, "D", 10.0, 0.0,
                   "DMA starts before later internal sentinels");
    require_null(sentinel_dma, 1, "D",
                 "DMA maps a directly selected X sentinel to missing");
    const float sentinel_recurrence = static_cast<float>(
        static_cast<double>(tcalc_missing_sentinel) * 0.5 + 30.0 * 0.5);
    require_number(sentinel_dma, 2, "D", sentinel_recurrence, 0.0,
                   "DMA retains the raw X sentinel as post-start float state");
    require_null(sentinel_dma, 3, "D",
                 "DMA safely maps overflow from an internal A sentinel to missing");
    require_number(sentinel_dma, 4, "D", 50.0, 0.0,
                   "DMA direct-source branch recovers after non-finite state");
    require_number(sentinel_dma, 5, "D", 55.0, 0.0,
                   "DMA resumes its float recurrence after direct recovery");

    auto leading_slope_sample = sample(6);
    set_close_series(leading_slope_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0, 60.0});
    const auto leading_slope = tdx::evaluate_formula_source_document(
        leading_slope_sample,
        "X:=IF(CURRBARSCOUNT>=5,DRAWNULL,CLOSE);S:SLOPE(X,3);",
        {}, "NATIVESLOPELEADING");
    require_null(leading_slope, 0, "S",
                 "SLOPE preserves its leading canonical sentinel");
    require_null(leading_slope, 1, "S",
                 "SLOPE scans to its first native float source");
    require_null(leading_slope, 2, "S",
                 "SLOPE measures available history from its first source");
    require_null(leading_slope, 3, "S",
                 "SLOPE rejects a window crossing its first source");
    require_number(leading_slope, 4, "S", 10.0, 0.0,
                   "SLOPE accepts its first complete native window");
    require_number(leading_slope, 5, "S", 10.0, 0.0,
                   "SLOPE advances its native window after startup");

    auto dynamic_slope_sample = sample(6);
    set_close_series(dynamic_slope_sample,
                     {1.0, 2.0, 10.0, 20.0, 50.0, 90.0});
    const auto dynamic_slope = tdx::evaluate_formula_source_document(
        dynamic_slope_sample,
        "N:=IF(CURRBARSCOUNT=6,0,IF(CURRBARSCOUNT=5,-2,"
        "IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,2147483648,"
        "IF(CURRBARSCOUNT=2,2.4969,2.497)))));S:SLOPE(CLOSE,N);",
        {}, "NATIVESLOPEDYNAMICPERIOD");
    for (std::size_t index = 0; index < 4; ++index)
        require_null(dynamic_slope, index, "S",
                     "SLOPE safely rejects non-positive, missing, and "
                     "out-of-range native periods");
    require_number(dynamic_slope, 4, "S", 30.0, 0.0,
                   "SLOPE rounds a period below the native bias boundary down");
    require_number(dynamic_slope, 5, "S", 35.000003814697265625, 0.0,
                   "SLOPE rounds a period at the native bias boundary up");

    auto sentinel_slope_sample = sample(5);
    set_close_series(sentinel_slope_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0});
    const auto sentinel_slope = tdx::evaluate_formula_source_document(
        sentinel_slope_sample,
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);S:SLOPE(X,2);",
        {}, "NATIVESLOPESENTINEL");
    require_null(sentinel_slope, 0, "S",
                 "SLOPE waits for its first complete window");
    require_number(sentinel_slope, 1, "S", 10.0, 0.0,
                   "SLOPE starts before a later internal sentinel");
    require_null(sentinel_slope, 2, "S",
                 "SLOPE maps an exact sentinel regression result to missing");
    require_number(sentinel_slope, 3, "S",
                   -static_cast<double>(tcalc_missing_sentinel), 0.0,
                   "SLOPE restores an internal missing source as its sentinel");
    require_number(sentinel_slope, 4, "S", 10.0, 0.0,
                   "SLOPE recovers after an internal sentinel leaves the window");

    auto float_slope_sample = sample(3);
    set_close_series(float_slope_sample,
                     {16777216.0, 1.0, -16777216.0});
    const auto float_slope = tdx::evaluate_formula_source_document(
        float_slope_sample, "S:SLOPE(CLOSE,3);", {}, "NATIVESLOPEFLOAT");
    require_null(float_slope, 0, "S",
                 "SLOPE needs its complete float regression window");
    require_null(float_slope, 1, "S",
                 "SLOPE keeps waiting for its float regression window");
    require_number(float_slope, 2, "S", -16777218.0, 0.0,
                   "SLOPE preserves backward f32 accumulation and landing points");

    auto final_quotient_slope_sample = sample(4);
    set_close_series(final_quotient_slope_sample,
                     {5941892.0, 49958348.0, -9642243.0,
                      1061.6112060546875});
    const auto final_quotient_slope = tdx::evaluate_formula_source_document(
        final_quotient_slope_sample, "S:SLOPE(CLOSE,4);", {},
        "NATIVESLOPEFINALQUOTIENT");
    require_number(final_quotient_slope, 3, "S", -7742307.5, 0.0,
                   "SLOPE keeps the final numerator and denominator wide until "
                   "the quotient is stored as float");

    const auto singular_slope = tdx::evaluate_formula_source_document(
        sample(3), "S:SLOPE(CLOSE,1);", {}, "NATIVESLOPESINGULAR");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(singular_slope, index, "S",
                     "SLOPE maps its native zero-denominator result to missing");

    auto dynamic_avedev_sample = sample(4);
    set_close_series(dynamic_avedev_sample, {1.0, 2.0, 3.0, 4.0});
    const auto dynamic_avedev = tdx::evaluate_formula_source_document(
        dynamic_avedev_sample,
        "N:=IF(CURRBARSCOUNT=4,1,IF(CURRBARSCOUNT=3,2,"
        "IF(CURRBARSCOUNT=2,100,3.9)));D:AVEDEV(CLOSE,N);",
        {}, "NATIVEAVEDEVFINALPERIOD");
    require_null(dynamic_avedev, 0, "D",
                 "AVEDEV uses the final-bar N instead of the current N");
    require_null(dynamic_avedev, 1, "D",
                 "AVEDEV waits for its final-bar native window");
    require_number(dynamic_avedev, 2, "D",
                   0.666666686534881591796875, 0.0,
                   "AVEDEV stores its first completed deviation through float");
    require_number(dynamic_avedev, 3, "D",
                   0.666666686534881591796875, 0.0,
                   "AVEDEV advances the float rolling mean");

    auto float_avedev_sample = sample(4);
    set_close_series(float_avedev_sample,
                     {16777216.0, 1.0, -16777216.0, 3.0});
    const auto float_avedev = tdx::evaluate_formula_source_document(
        float_avedev_sample, "D:AVEDEV(CLOSE,3);", {},
        "NATIVEAVEDEVFLOAT");
    require_null(float_avedev, 0, "D",
                 "AVEDEV needs its complete native window");
    require_null(float_avedev, 1, "D",
                 "AVEDEV preserves the incomplete-window prefix");
    require_number(float_avedev, 2, "D", 11184810.0, 0.0,
                   "AVEDEV applies each seed-mean and deviation float landing point");
    require_number(float_avedev, 3, "D", 7456541.0, 0.0,
                   "AVEDEV sums rolling deviations newest to oldest in wide precision");

    auto leading_avedev_sample = sample(5);
    set_close_series(leading_avedev_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0});
    const auto leading_avedev = tdx::evaluate_formula_source_document(
        leading_avedev_sample,
        "X:=IF(CURRBARSCOUNT>=4,DRAWNULL,CLOSE);D:AVEDEV(X,2);",
        {}, "NATIVEAVEDEVLEADING");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(leading_avedev, index, "D",
                     "AVEDEV measures its first window after the leading source gate");
    require_number(leading_avedev, 3, "D", 5.0, 0.0,
                   "AVEDEV emits at the first complete post-gate window");
    require_number(leading_avedev, 4, "D", 5.0, 0.0,
                   "AVEDEV rolls forward from its post-gate seed");

    auto sentinel_avedev_sample = sample(4);
    set_close_series(sentinel_avedev_sample, {10.0, 20.0, 30.0, 40.0});
    const auto sentinel_avedev = tdx::evaluate_formula_source_document(
        sentinel_avedev_sample,
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);D:AVEDEV(X,2);",
        {}, "NATIVEAVEDEVSENTINEL");
    require_null(sentinel_avedev, 0, "D",
                 "AVEDEV waits for its first complete window");
    require_number(sentinel_avedev, 1, "D", 5.0, 0.0,
                   "AVEDEV starts before a later source sentinel");
    const double half_sentinel =
        -static_cast<double>(tcalc_missing_sentinel) / 2.0;
    require_number(sentinel_avedev, 2, "D", half_sentinel, 0.0,
                   "AVEDEV includes an internal sentinel in its rolling window");
    require_number(sentinel_avedev, 3, "D", half_sentinel, 0.0,
                   "AVEDEV retains the sentinel-derived float rolling mean");

    auto carried_power_sample = sample(5);
    set_close_series(carried_power_sample,
                     {2.0, -4.0, 0.0, 10.0, 16777217.0});
    const auto carried_power = tdx::evaluate_formula_source_document(
        carried_power_sample,
        "E:=IF(CURRBARSCOUNT=5,3,IF(CURRBARSCOUNT=4,0.5,"
        "IF(CURRBARSCOUNT=3,0,IF(CURRBARSCOUNT=2,39,2))));"
        "P:POW(CLOSE,E);",
        {}, "NATIVEPOWCARRY");
    for (std::size_t index = 0; index < 4; ++index)
        require_number(carried_power, index, "P", 8.0, 0.0,
                       "POW carries its previous float output across domain and range failures");
    require_number(carried_power, 4, "P", 281474976710656.0, 0.0,
                   "POW narrows its base before evaluating and stores a float result");

    const auto mixed_range_power = tdx::evaluate_formula_source_document(
        sample(2),
        "B:=IF(CURRBARSCOUNT=2,2,1.01);"
        "E:=IF(CURRBARSCOUNT=2,3,8793.5048828125);P:POW(B,E);",
        {}, "NATIVEPOWMIXEDRANGEGATE");
    require_number(mixed_range_power, 0, "P", 8.0, 0.0,
                   "POW seeds a value before the output-range discriminator");
    require_number(mixed_range_power, 1, "P", 8.0, 0.0,
                   "POW keeps the logarithm product wide while its tolerance magnitude is float");

    auto zero_power_sample = sample(2);
    set_close_series(zero_power_sample, {0.0, 2.0});
    const auto zero_power = tdx::evaluate_formula_source_document(
        zero_power_sample, "P:POW(CLOSE,CLOSE);", {},
        "NATIVEPOWZEROCARRY");
    require_null(zero_power, 0, "P",
                 "POW leaves a leading zero-to-zero domain failure missing");
    require_number(zero_power, 1, "P", 4.0, 0.0,
                   "POW recovers after a leading domain failure");

    auto integer_power_sample = sample(4);
    set_close_series(integer_power_sample, {-4.0, -4.0, 0.0, 0.0});
    const auto integer_power = tdx::evaluate_formula_source_document(
        integer_power_sample,
        "E:=IF(CURRBARSCOUNT=4,3,IF(CURRBARSCOUNT=3,2,"
        "IF(CURRBARSCOUNT=2,1,0)));P:POW(CLOSE,E);",
        {}, "NATIVEPOWINTEGER");
    const double integer_power_values[]{-64.0, 16.0, 0.0, 0.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(integer_power, index, "P",
                       integer_power_values[index], 0.0,
                       "POW accepts safe integer exponents and carries zero-to-zero");

    auto leading_power_sample = sample(3);
    set_close_series(leading_power_sample, {0.0, 0.0, 2.0});
    const auto leading_power = tdx::evaluate_formula_source_document(
        leading_power_sample,
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);P:POW(X,X);",
        {}, "NATIVEPOWLEADING");
    require_null(leading_power, 0, "P",
                 "POW skips a leading dual-sentinel operand pair");
    require_null(leading_power, 1, "P",
                 "POW carries the leading sentinel through zero-to-zero");
    require_number(leading_power, 2, "P", 4.0, 0.0,
                   "POW starts after its dual-sentinel gate and invalid carry");

    auto sentinel_power_sample = sample(8);
    set_close_series(sentinel_power_sample,
                     {1.0, 2.0, 2.0, 1.0, -4.0, 2.0, 0.0, 3.0});
    const auto sentinel_power = tdx::evaluate_formula_source_document(
        sentinel_power_sample,
        "B:=IF(CURRBARSCOUNT=8 OR CURRBARSCOUNT=5,DRAWNULL,CLOSE);"
        "E:=IF(CURRBARSCOUNT=7 OR CURRBARSCOUNT=3,DRAWNULL,"
        "IF(CURRBARSCOUNT=6,3,IF(CURRBARSCOUNT=5,1,"
        "IF(CURRBARSCOUNT=4,0.5,IF(CURRBARSCOUNT=2,0,2)))));"
        "P:POW(B,E);",
        {}, "NATIVEPOWSENTINELSTATE");
    require_null(sentinel_power, 0, "P",
                 "POW waits across a one-sided leading base sentinel");
    require_null(sentinel_power, 1, "P",
                 "POW waits across a one-sided leading exponent sentinel");
    require_number(sentinel_power, 2, "P", 8.0, 0.0,
                   "POW starts at its first jointly non-sentinel operands");
    require_null(sentinel_power, 3, "P",
                 "POW evaluates a post-start base sentinel as raw float");
    require_null(sentinel_power, 4, "P",
                 "POW carries a sentinel result across a domain failure");
    require_number(sentinel_power, 5, "P", 0.0, 0.0,
                   "POW evaluates a post-start exponent sentinel as raw float");
    require_number(sentinel_power, 6, "P", 0.0, 0.0,
                   "POW carries the raw zero result across zero-to-zero");
    require_number(sentinel_power, 7, "P", 9.0, 0.0,
                   "POW recovers after post-start sentinel state");

    auto boundary_refx_sample = sample(5);
    set_close_series(boundary_refx_sample, {1.0, 10.0, 20.0, 30.0, 40.0});
    const auto boundary_refx = tdx::evaluate_formula_source_document(
        boundary_refx_sample,
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,CLOSE);"
        "N:=IF(CURRBARSCOUNT=5,0,IF(CURRBARSCOUNT=4,1,"
        "IF(CURRBARSCOUNT>=2,2,0)));R:REFX(X,N);V:REFXV(X,N);",
        {}, "NATIVEREFXBOUNDARY");
    require_null(boundary_refx, 0, "R",
                 "REFX skips its jointly invalid leading bar");
    require_null(boundary_refx, 0, "V",
                 "REFXV skips its jointly invalid leading bar");
    require_number(boundary_refx, 1, "R", 20.0, 0.0,
                   "REFX selects its forward raw source");
    require_number(boundary_refx, 1, "V", 20.0, 0.0,
                   "REFXV selects its forward raw source");
    require_number(boundary_refx, 2, "R", 40.0, 0.0,
                   "REFX accepts the final in-range target");
    require_number(boundary_refx, 2, "V", 40.0, 0.0,
                   "REFXV accepts the final in-range target");
    require_null(boundary_refx, 3, "R",
                 "REFX writes a sentinel beyond its right boundary");
    require_number(boundary_refx, 3, "V", 40.0, 0.0,
                   "REFXV carries its previous raw output beyond the boundary");
    require_number(boundary_refx, 4, "R", 40.0, 0.0,
                   "REFX resumes with a valid zero offset");
    require_number(boundary_refx, 4, "V", 40.0, 0.0,
                   "REFXV resumes with a valid zero offset");

    auto leading_refx_sample = sample(4);
    set_close_series(leading_refx_sample, {10.0, 20.0, 30.0, 40.0});
    const auto leading_refx = tdx::evaluate_formula_source_document(
        leading_refx_sample,
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);"
        "N:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=2,1,0));R:REFX(X,N);V:REFXV(X,N);",
        {}, "NATIVEREFXLEADING");
    for (std::size_t index = 0; index < 2; ++index) {
        require_null(leading_refx, index, "R",
                     "REFX waits while either leading operand is sentinel");
        require_null(leading_refx, index, "V",
                     "REFXV waits while either leading operand is sentinel");
    }
    for (std::size_t index = 2; index < 4; ++index) {
        require_number(leading_refx, index, "R", 40.0, 0.0,
                       "REFX starts at its first jointly valid operands");
        require_number(leading_refx, index, "V", 40.0, 0.0,
                       "REFXV starts at its first jointly valid operands");
    }

    auto sentinel_refx_sample = sample(4);
    set_close_series(sentinel_refx_sample, {10.0, 20.0, 30.0, 40.0});
    const auto sentinel_refx = tdx::evaluate_formula_source_document(
        sentinel_refx_sample,
        "N:=IF(CURRBARSCOUNT=2,DRAWNULL,"
        "IF(CURRBARSCOUNT>=3,1,0));R:REFX(CLOSE,N);V:REFXV(CLOSE,N);",
        {}, "NATIVEREFXSENTINEL");
    require_number(sentinel_refx, 0, "R", 20.0, 0.0,
                   "REFX starts with a valid forward selection");
    require_number(sentinel_refx, 0, "V", 20.0, 0.0,
                   "REFXV starts with a valid forward selection");
    require_number(sentinel_refx, 1, "R", 30.0, 0.0,
                   "REFX advances its valid forward selection");
    require_number(sentinel_refx, 1, "V", 30.0, 0.0,
                   "REFXV advances its valid forward selection");
    require_null(sentinel_refx, 2, "R",
                 "REFX treats a post-start offset sentinel as invalid");
    require_number(sentinel_refx, 2, "V", 30.0, 0.0,
                   "REFXV carries across a post-start offset sentinel");
    require_number(sentinel_refx, 3, "R", 40.0, 0.0,
                   "REFX recovers after an offset sentinel");
    require_number(sentinel_refx, 3, "V", 40.0, 0.0,
                   "REFXV recovers after an offset sentinel");

    auto tolerance_refx_sample = sample(3);
    set_close_series(tolerance_refx_sample, {10.0, 20.0, 30.0});
    const auto tolerance_refx = tdx::evaluate_formula_source_document(
        tolerance_refx_sample,
        "N:=IF(CURRBARSCOUNT=3,-0.00002,"
        "IF(CURRBARSCOUNT=2,-0.000005,1));"
        "R:REFX(CLOSE,N);V:REFXV(CLOSE,N);",
        {}, "NATIVEREFXTOLERANCE");
    require_null(tolerance_refx, 0, "R",
                 "REFX rejects an offset below its negative tolerance");
    require_number(tolerance_refx, 0, "V", 10.0, 0.0,
                   "REFXV falls back to source for an invalid index-zero offset");
    require_number(tolerance_refx, 1, "R", 20.0, 0.0,
                   "REFX accepts a small negative offset that truncates to zero");
    require_number(tolerance_refx, 1, "V", 20.0, 0.0,
                   "REFXV accepts a small negative offset that truncates to zero");
    require_null(tolerance_refx, 2, "R",
                 "REFX applies its tolerant right-boundary gate");
    require_number(tolerance_refx, 2, "V", 20.0, 0.0,
                   "REFXV carries across a tolerant right-boundary failure");

    auto float_refx_sample = sample(3);
    set_close_series(float_refx_sample, {10.0, 20.0, 16777217.0});
    const auto float_refx = tdx::evaluate_formula_source_document(
        float_refx_sample,
        "N:=IF(CURRBARSCOUNT=3,0.99999999,0);"
        "R:REFX(CLOSE,N);V:REFXV(CLOSE,N);",
        {}, "NATIVEREFXFLOAT");
    const double float_refx_values[]{20.0, 20.0, 16777216.0};
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(float_refx, index, "R", float_refx_values[index], 0.0,
                       "REFX narrows offsets and selected sources through float");
        require_number(float_refx, index, "V", float_refx_values[index], 0.0,
                       "REFXV narrows offsets and selected sources through float");
    }

    auto native_ref_sample = sample(4);
    set_close_series(native_ref_sample, {10.0, 20.0, 30.0, 16777217.0});
    const auto native_ref = tdx::evaluate_formula_source_document(
        native_ref_sample,
        "X:=IF(CURRBARSCOUNT=4,-4.0398103E34,CLOSE);"
        "N:=IF(CURRBARSCOUNT=4,1,IF(CURRBARSCOUNT=1,DRAWNULL,1));"
        "R:REF(X,N);F:REF(CLOSE,1);",
        {}, "NATIVEREFRAWSENTINEL");
    require_null(native_ref, 0, "R",
                 "REF skips jointly invalid leading raw-f32 operands");
    require_null(native_ref, 1, "R",
                 "REF maps a selected canonical source sentinel to missing");
    require_number(native_ref, 2, "R", 20.0, 0.0,
                   "REF resumes with its prior raw-f32 source");
    require_number(native_ref, 3, "R", 20.0, 0.0,
                   "REF carries the previous raw output for a sentinel offset");
    require_null(native_ref, 0, "F",
                 "REF preserves its native left-boundary prefix");
    require_number(native_ref, 3, "F", 30.0, 0.0,
                   "REF selects and publishes a raw-f32 source value");

    auto native_value_when_sample = sample(9);
    set_close_series(native_value_when_sample,
                     {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0,
                      80.0, 16777217.0});
    const auto native_value_when = tdx::evaluate_formula_source_document(
        native_value_when_sample,
        "Q:=IF(CURRBARSCOUNT=9 OR CURRBARSCOUNT=5,DRAWNULL,"
        "CURRBARSCOUNT=7 OR CURRBARSCOUNT=3 OR CURRBARSCOUNT=1);"
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);V:VALUEWHEN(Q,X);",
        {}, "NATIVEVALUEWHENSTATE");
    require_null(native_value_when, 0, "V",
                 "VALUEWHEN skips its leading condition sentinel");
    require_null(native_value_when, 1, "V",
                 "VALUEWHEN carries the leading sentinel on exact zero");
    require_number(native_value_when, 2, "V", 30.0, 0.0,
                   "VALUEWHEN selects on an exact non-zero condition");
    require_number(native_value_when, 3, "V", 30.0, 0.0,
                   "VALUEWHEN carries its raw float selection on zero");
    require_number(native_value_when, 4, "V", 50.0, 0.0,
                   "VALUEWHEN treats a post-start condition sentinel as non-zero");
    require_number(native_value_when, 5, "V", 50.0, 0.0,
                   "VALUEWHEN carries a value selected by a condition sentinel");
    require_null(native_value_when, 6, "V",
                 "VALUEWHEN lets a selected sentinel overwrite held state");
    require_null(native_value_when, 7, "V",
                 "VALUEWHEN carries an overwritten sentinel on zero");
    require_number(native_value_when, 8, "V", 16777216.0, 0.0,
                   "VALUEWHEN narrows a selected value through native float");

    auto exact_zero_value_when_sample = sample(3);
    set_close_series(exact_zero_value_when_sample, {1.0, 2.0, 3.0});
    const auto exact_zero_value_when =
        tdx::evaluate_formula_source_document(
            exact_zero_value_when_sample,
            "Q:=IF(CURRBARSCOUNT=3,0,"
            "IF(CURRBARSCOUNT=2,1E-20,-0.0));V:VALUEWHEN(Q,CLOSE);",
            {}, "NATIVEVALUEWHENEXACTZERO");
    require_null(exact_zero_value_when, 0, "V",
                 "VALUEWHEN has no value to carry from an index-zero zero");
    require_number(exact_zero_value_when, 1, "V", 2.0, 0.0,
                   "VALUEWHEN selects on a tiny but exact non-zero float");
    require_number(exact_zero_value_when, 2, "V", 2.0, 0.0,
                   "VALUEWHEN treats native negative zero as exact zero");

    auto missing_value_when_sample = sample(3);
    set_close_series(missing_value_when_sample, {1.0, 2.0, 3.0});
    const auto missing_value_when = tdx::evaluate_formula_source_document(
        missing_value_when_sample,
        "Q:=CURRBARSCOUNT=3 OR CURRBARSCOUNT=1;"
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);V:VALUEWHEN(Q,X);",
        {}, "NATIVEVALUEWHENSELECTEDMISSING");
    require_null(missing_value_when, 0, "V",
                 "VALUEWHEN selects and stores a missing value");
    require_null(missing_value_when, 1, "V",
                 "VALUEWHEN carries the selected missing value");
    require_number(missing_value_when, 2, "V", 3.0, 0.0,
                   "VALUEWHEN recovers when a later selection is valid");

    auto native_between_sample = sample(8);
    set_close_series(native_between_sample,
                     {5.0, 9.0, 9.0, -0.000005, -0.00002,
                      10.000005, 7.0, 5.0});
    const auto native_between = tdx::evaluate_formula_source_document(
        native_between_sample,
        "V:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
        "BA:=IF(CURRBARSCOUNT>=6 OR CURRBARSCOUNT=1,DRAWNULL,0);"
        "BB:=IF(CURRBARSCOUNT=8 OR CURRBARSCOUNT=6 OR "
        "CURRBARSCOUNT=1,DRAWNULL,10);R:BETWEEN(V,BA,BB);",
        {}, "NATIVEBETWEENSTATE");
    require_null(native_between, 0, "R",
                 "BETWEEN skips a leading pair of bound sentinels");
    const double native_between_values[]{1.0, 0.0, 1.0, 0.0,
                                         1.0, 0.0, 0.0};
    for (std::size_t index = 1; index < 8; ++index)
        require_number(native_between, index, "R",
                       native_between_values[index - 1], 0.0,
                       "BETWEEN retains raw float sentinels and open tolerance bounds after startup");

    auto missing_value_between_sample = sample(2);
    set_close_series(missing_value_between_sample, {1.0, 5.0});
    const auto missing_value_between =
        tdx::evaluate_formula_source_document(
            missing_value_between_sample,
            "V:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);R:BETWEEN(V,0,10);",
            {}, "NATIVEBETWEENVALUEGATE");
    require_number(missing_value_between, 0, "R", 0.0, 0.0,
                   "BETWEEN does not include value in its leading gate");
    require_number(missing_value_between, 1, "R", 1.0, 0.0,
                   "BETWEEN continues after a leading value sentinel");

    const auto float_between = tdx::evaluate_formula_source_document(
        sample(1),
        "R:BETWEEN(16777217,16777216,16777216);Q:BETWEEN(5,10,0);",
        {}, "NATIVEBETWEENFLOAT");
    require_number(float_between, 0, "R", 1.0, 0.0,
                   "BETWEEN narrows its value and bounds through native float");
    require_number(float_between, 0, "Q", 1.0, 0.0,
                   "BETWEEN orders reversed native float bounds");

    auto tolerance_between_sample = sample(3);
    set_close_series(tolerance_between_sample,
                     {-0.000005, -0.00002, 10.000005});
    const auto tolerance_between = tdx::evaluate_formula_source_document(
        tolerance_between_sample, "R:BETWEEN(CLOSE,0,10);", {},
        "NATIVEBETWEENTOLERANCE");
    const double tolerance_between_values[]{1.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(tolerance_between, index, "R",
                       tolerance_between_values[index], 0.0,
                       "BETWEEN applies its float-magnitude relative and absolute open tolerance");

    auto final_period_last_sample = sample(6);
    set_close_series(final_period_last_sample, {1.0, 1.0, 0.0, 1.0, 1.0, 1.0});
    const auto final_period_last = tdx::evaluate_formula_source_document(
        final_period_last_sample,
        "A:=IF(CURRBARSCOUNT=1,0,5);"
        "B:=IF(CURRBARSCOUNT=1,2,0);L:LAST(CLOSE,A,B);",
        {}, "NATIVELASTFINALPERIOD");
    const double final_period_last_values[]{1.0, 1.0, 1.0, 1.0, 0.0, 0.0};
    for (std::size_t index = 0; index < 6; ++index)
        require_number(final_period_last, index, "L",
                       final_period_last_values[index], 0.0,
                       "LAST uses final offsets and zero begin as all history");

    auto sentinel_last_sample = sample(6);
    set_close_series(sentinel_last_sample, {1.0, 1.0, 0.0, 1.0, 1.0, 1.0});
    const auto sentinel_last = tdx::evaluate_formula_source_document(
        sentinel_last_sample,
        "X:=IF(CURRBARSCOUNT=6 OR CURRBARSCOUNT=3,DRAWNULL,CLOSE);"
        "L:LAST(X,2,0);",
        {}, "NATIVELASTSENTINEL");
    require_null(sentinel_last, 0, "L",
                 "LAST preserves its leading source sentinel");
    const double sentinel_last_values[]{1.0, 0.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 1; index < 6; ++index)
        require_number(sentinel_last, index, "L",
                       sentinel_last_values[index - 1], 0.0,
                       "LAST scans an inclusive native window and treats an internal sentinel as true");

    auto epsilon_last_sample = sample(5);
    set_close_series(epsilon_last_sample,
                     {0.00001, 0.0000099, -0.00001, -0.0000099, 1.0});
    const auto epsilon_last = tdx::evaluate_formula_source_document(
        epsilon_last_sample, "L:LAST(CLOSE,1,1);", {},
        "NATIVELASTEPSILON");
    const double epsilon_last_values[]{1.0, 1.0, 0.0, 1.0, 0.0};
    for (std::size_t index = 0; index < 5; ++index)
        require_number(epsilon_last, index, "L",
                       epsilon_last_values[index], 0.0,
                       "LAST uses strict native float zero boundaries");

    const auto native_nday = tdx::evaluate_formula_source_document(
        sample(8),
        "X:=IF(CURRBARSCOUNT=8 OR CURRBARSCOUNT=3,DRAWNULL,10);"
        "Y:=IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=6,9.999995,"
        "IF(CURRBARSCOUNT=5 OR CURRBARSCOUNT<=2,9.99998,0)));"
        "N:=IF(CURRBARSCOUNT=1,2,1);D:NDAY(X,Y,N);",
        {}, "NATIVENDAYSTATE");
    require_null(native_nday, 0, "D",
                 "NDAY waits for its first jointly valid operands");
    require_null(native_nday, 1, "D",
                 "NDAY waits for the final-bar native window");
    const double native_nday_values[]{0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 2; index < 8; ++index)
        require_number(native_nday, index, "D",
                       native_nday_values[index - 2], 0.0,
                       "NDAY uses tolerant raw-float comparisons and a consecutive run");

    auto float_nday_sample = sample(4);
    set_close_series(float_nday_sample, {0.0, 0.0, 16777217.0, 10.0});
    const auto float_nday = tdx::evaluate_formula_source_document(
        float_nday_sample,
        "Y:=IF(CURRBARSCOUNT=4,-0.00001,"
        "IF(CURRBARSCOUNT=3,-0.0000099,"
        "IF(CURRBARSCOUNT=2,16777216,9.99998)));"
        "D:NDAY(CLOSE,Y,1);",
        {}, "NATIVENDAYFLOAT");
    const double float_nday_values[]{1.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(float_nday, index, "D", float_nday_values[index], 0.0,
                       "NDAY applies its open tolerance after native float narrowing");

    auto up_run_sample = sample(7);
    set_close_series(up_run_sample, {0.0, 1.0, 2.0, 3.0, 2.0, 3.0, 4.0});
    const auto up_run = tdx::evaluate_formula_source_document(
        up_run_sample,
        "X:=IF(CURRBARSCOUNT=7,DRAWNULL,CLOSE);"
        "N:=IF(CURRBARSCOUNT=1,2.9,1);U:UPNDAY(X,N);",
        {}, "NATIVEUPNDAYRUN");
    require_null(up_run, 0, "U",
                 "UPNDAY preserves its leading source sentinel");
    require_null(up_run, 1, "U",
                 "UPNDAY leaves the baseline before its first output missing");
    const double up_run_values[]{0.0, 1.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 2; index < 7; ++index)
        require_number(up_run, index, "U", up_run_values[index - 2], 0.0,
                       "UPNDAY uses final float-truncated N and a consecutive run");

    auto down_run_sample = sample(7);
    set_close_series(down_run_sample, {0.0, 4.0, 3.0, 2.0, 3.0, 2.0, 1.0});
    const auto down_run = tdx::evaluate_formula_source_document(
        down_run_sample,
        "X:=IF(CURRBARSCOUNT=7,DRAWNULL,CLOSE);"
        "N:=IF(CURRBARSCOUNT=1,2.9,1);D:DOWNNDAY(X,N);",
        {}, "NATIVEDOWNNDAYRUN");
    require_null(down_run, 0, "D",
                 "DOWNNDAY preserves its leading source sentinel");
    require_null(down_run, 1, "D",
                 "DOWNNDAY leaves the baseline before its first output missing");
    const double down_run_values[]{0.0, 1.0, 0.0, 0.0, 1.0};
    for (std::size_t index = 2; index < 7; ++index)
        require_number(down_run, index, "D",
                       down_run_values[index - 2], 0.0,
                       "DOWNNDAY uses final float-truncated N and a consecutive run");

    auto sentinel_directional_sample = sample(5);
    set_close_series(sentinel_directional_sample,
                     {1.0, 2.0, 3.0, 3.0, 4.0});
    const auto sentinel_directional =
        tdx::evaluate_formula_source_document(
            sentinel_directional_sample,
            "X:=IF(CURRBARSCOUNT=3,DRAWNULL,CLOSE);"
            "U:UPNDAY(X,1);D:DOWNNDAY(X,1);",
            {}, "NATIVEDIRECTIONALNDAYSENTINEL");
    const double sentinel_up_values[]{0.0, 1.0, 0.0, 1.0, 1.0};
    const double sentinel_down_values[]{0.0, 0.0, 1.0, 0.0, 0.0};
    for (std::size_t index = 0; index < 5; ++index) {
        require_number(sentinel_directional, index, "U",
                       sentinel_up_values[index], 0.0,
                       "UPNDAY compares a post-start sentinel as raw float");
        require_number(sentinel_directional, index, "D",
                       sentinel_down_values[index], 0.0,
                       "DOWNNDAY compares a post-start sentinel as raw float");
    }

    auto up_epsilon_sample = sample(6);
    set_close_series(up_epsilon_sample,
                     {-0.00001, 0.0, -0.0000099, 0.0, -0.0000101, 0.0});
    const auto up_epsilon = tdx::evaluate_formula_source_document(
        up_epsilon_sample, "U:UPNDAY(CLOSE,1);", {},
        "NATIVEUPNDAYEPSILON");
    auto down_epsilon_sample = sample(6);
    set_close_series(down_epsilon_sample,
                     {0.00001, 0.0, 0.0000099, 0.0, 0.0000101, 0.0});
    const auto down_epsilon = tdx::evaluate_formula_source_document(
        down_epsilon_sample, "D:DOWNNDAY(CLOSE,1);", {},
        "NATIVEDOWNNDAYEPSILON");
    const double directional_epsilon_values[]{0.0, 1.0, 0.0,
                                               0.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 6; ++index) {
        require_number(up_epsilon, index, "U",
                       directional_epsilon_values[index], 0.0,
                       "UPNDAY accepts equality at its strict float tolerance boundary");
        require_number(down_epsilon, index, "D",
                       directional_epsilon_values[index], 0.0,
                       "DOWNNDAY accepts equality at its strict float tolerance boundary");
    }

    auto leading_low_range_sample = sample(5);
    set_close_series(leading_low_range_sample, {10.0, 9.0, 8.0, 7.0, 6.0});
    const auto leading_low_range = tdx::evaluate_formula_source_document(
        leading_low_range_sample,
        "X:=IF(CURRBARSCOUNT>=4,DRAWNULL,CLOSE);R:LOWRANGE(X);",
        {}, "NATIVELOWRANGELEADING");
    require_null(leading_low_range, 0, "R",
                 "LOWRANGE preserves its leading source sentinel");
    require_null(leading_low_range, 1, "R",
                 "LOWRANGE waits for its first valid source bar");
    const double leading_low_range_values[]{0.0, 1.0, 2.0};
    for (std::size_t index = 2; index < 5; ++index)
        require_number(leading_low_range, index, "R",
                       leading_low_range_values[index - 2], 0.0,
                       "LOWRANGE scans prior bars until it reaches a raw leading sentinel");

    auto sentinel_low_range_sample = sample(4);
    set_close_series(sentinel_low_range_sample, {10.0, 9.0, 8.0, 7.0});
    const auto sentinel_low_range = tdx::evaluate_formula_source_document(
        sentinel_low_range_sample,
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);R:LOWRANGE(X);",
        {}, "NATIVELOWRANGESENTINEL");
    const double sentinel_low_range_values[]{0.0, 1.0, 2.0, 0.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(sentinel_low_range, index, "R",
                       sentinel_low_range_values[index], 0.0,
                       "LOWRANGE treats a post-start sentinel as raw float");

    auto float_low_range_sample = sample(3);
    set_close_series(float_low_range_sample,
                     {16777217.0, 16777216.0, 16777214.0});
    const auto float_low_range = tdx::evaluate_formula_source_document(
        float_low_range_sample, "R:LOWRANGE(CLOSE);", {},
        "NATIVELOWRANGEFLOAT");
    const double float_low_range_values[]{0.0, 0.0, 2.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(float_low_range, index, "R",
                       float_low_range_values[index], 0.0,
                       "LOWRANGE narrows current and prior values through native float");

    auto epsilon_low_range_sample = sample(3);
    set_close_series(epsilon_low_range_sample,
                     {0.00001, 0.0, -0.0000101});
    const auto epsilon_low_range = tdx::evaluate_formula_source_document(
        epsilon_low_range_sample, "R:LOWRANGE(CLOSE);", {},
        "NATIVELOWRANGEEPSILON");
    const double epsilon_low_range_values[]{0.0, 0.0, 2.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(epsilon_low_range, index, "R",
                       epsilon_low_range_values[index], 0.0,
                       "LOWRANGE uses a strict native epsilon boundary");

    auto leading_top_range_sample = sample(5);
    set_close_series(leading_top_range_sample, {10.0, 11.0, 12.0, 13.0, 14.0});
    const auto leading_top_range = tdx::evaluate_formula_source_document(
        leading_top_range_sample,
        "X:=IF(CURRBARSCOUNT>=4,DRAWNULL,CLOSE);R:TOPRANGE(X);",
        {}, "NATIVETOPRANGELEADING");
    require_null(leading_top_range, 0, "R",
                 "TOPRANGE preserves its leading source sentinel");
    require_null(leading_top_range, 1, "R",
                 "TOPRANGE waits for its first valid source bar");
    const double leading_top_range_values[]{2.0, 3.0, 4.0};
    for (std::size_t index = 2; index < 5; ++index)
        require_number(
            leading_top_range, index, "R",
            leading_top_range_values[index - 2], 0.0,
            "TOPRANGE scans raw leading sentinels after its start gate");

    auto sentinel_top_range_sample = sample(4);
    set_close_series(sentinel_top_range_sample, {10.0, 11.0, 12.0, 13.0});
    const auto sentinel_top_range = tdx::evaluate_formula_source_document(
        sentinel_top_range_sample,
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);R:TOPRANGE(X);",
        {}, "NATIVETOPRANGESENTINEL");
    const double sentinel_top_range_values[]{0.0, 1.0, 0.0, 3.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(sentinel_top_range, index, "R",
                       sentinel_top_range_values[index], 0.0,
                       "TOPRANGE treats a post-start sentinel as raw float");

    auto float_top_range_sample = sample(3);
    set_close_series(float_top_range_sample,
                     {16777216.0, 16777217.0, 16777218.0});
    const auto float_top_range = tdx::evaluate_formula_source_document(
        float_top_range_sample, "R:TOPRANGE(CLOSE);", {},
        "NATIVETOPRANGEFLOAT");
    const double float_top_range_values[]{0.0, 0.0, 2.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(float_top_range, index, "R",
                       float_top_range_values[index], 0.0,
                       "TOPRANGE narrows current and prior values through native float");

    auto epsilon_top_range_sample = sample(3);
    set_close_series(epsilon_top_range_sample,
                     {0.0, 0.00001, 0.0000201});
    const auto epsilon_top_range = tdx::evaluate_formula_source_document(
        epsilon_top_range_sample, "R:TOPRANGE(CLOSE);", {},
        "NATIVETOPRANGEEPSILON");
    const double epsilon_top_range_values[]{0.0, 0.0, 2.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(epsilon_top_range, index, "R",
                       epsilon_top_range_values[index], 0.0,
                       "TOPRANGE uses a strict native epsilon boundary");

    auto binary_extreme_sample = sample(5);
    set_close_series(binary_extreme_sample,
                     {16777217.0, 20.0, 30.0, 40.0, 50.0});
    const auto binary_extremes = tdx::evaluate_formula_source_document(
        binary_extreme_sample,
        "L:=IF(CURRBARSCOUNT=5 OR CURRBARSCOUNT=1,DRAWNULL,CLOSE);"
        "R:=IF(CURRBARSCOUNT=4 OR CURRBARSCOUNT=2,DRAWNULL,"
        "IF(CURRBARSCOUNT=5,0,25));"
        "H:MAX(L,R);N:MIN(L,R);F:MAX(CLOSE,16777216);"
        "V:MAX(L,R,35);W:MIN(L,R,35);",
        {}, "NATIVEBINARYEXTREME");
    require_null(binary_extremes, 0, "H",
                 "MAX waits for its first jointly valid operands");
    require_null(binary_extremes, 0, "N",
                 "MIN waits for its first jointly valid operands");
    require_null(binary_extremes, 1, "H",
                 "MAX keeps waiting across one-sided leading missing");
    require_null(binary_extremes, 1, "N",
                 "MIN keeps waiting across one-sided leading missing");
    require_number(binary_extremes, 2, "H", 30.0, 0.0,
                   "MAX starts on the first jointly valid bar");
    require_number(binary_extremes, 2, "N", 25.0, 0.0,
                   "MIN starts on the first jointly valid bar");
    require_number(binary_extremes, 3, "H", 40.0, 0.0,
                   "MAX compares a later right missing sentinel as a float");
    require_null(binary_extremes, 3, "N",
                 "MIN selects a later right missing sentinel");
    require_number(binary_extremes, 4, "H", 25.0, 0.0,
                   "MAX compares a later left missing sentinel as a float");
    require_null(binary_extremes, 4, "N",
                 "MIN selects a later left missing sentinel");
    require_number(binary_extremes, 0, "F", 16777216.0, 0.0,
                   "MAX narrows both operands through native float");
    require_null(binary_extremes, 0, "V",
                 "variadic MAX preserves binary-fold startup semantics");
    require_number(binary_extremes, 2, "V", 35.0, 0.0,
                   "variadic MAX remains an explicit binary left fold");
    require_null(binary_extremes, 0, "W",
                 "variadic MIN preserves binary-fold startup semantics");
    require_number(binary_extremes, 2, "W", 25.0, 0.0,
                   "variadic MIN remains an explicit binary left fold");

    const auto all_missing_extremes = tdx::evaluate_formula_source_document(
        sample(3), "H:MAX(DRAWNULL,1);L:MIN(1,DRAWNULL);",
        {}, "NATIVEBINARYEXTREMEMISSING");
    for (std::size_t index = 0; index < 3; ++index) {
        require_null(all_missing_extremes, index, "H",
                     "MAX leaves a pair that never starts missing");
        require_null(all_missing_extremes, index, "L",
                     "MIN leaves a pair that never starts missing");
    }

    const auto equal_extremes = tdx::evaluate_formula_source_document(
        sample(1),
        "HR:MAX(0,-0.0);HL:MAX(-0.0,0);"
        "LR:MIN(0,-0.0);LL:MIN(-0.0,0);",
        {}, "NATIVEBINARYEXTREMEEQUALITY");
    require_zero_sign(equal_extremes, 0, "HR", true,
                      "MAX selects its right operand on float equality");
    require_zero_sign(equal_extremes, 0, "HL", false,
                      "MAX equality does not retain its left operand");
    require_zero_sign(equal_extremes, 0, "LR", true,
                      "MIN selects its right operand on float equality");
    require_zero_sign(equal_extremes, 0, "LL", false,
                      "MIN equality does not retain its left operand");

    auto extreme_bars_sample = sample(3);
    set_close_series(extreme_bars_sample, {1.0, 3.0, 2.0});
    const auto extreme_bars = tdx::evaluate_formula_source_document(
        extreme_bars_sample,
        "H:HHVBARS(CLOSE,5);L:LLVBARS(CLOSE,5);"
        "N:=IF(CURRBARSCOUNT=2,DRAWNULL,2);"
        "HD:HHVBARS(CLOSE,N);LD:LLVBARS(CLOSE,N);",
        {}, "NATIVEEXTREMEBARSWINDOW");
    const double highest_bars[]{0.0, 0.0, 1.0};
    const double lowest_bars[]{0.0, 1.0, 2.0};
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(extreme_bars, index, "H", highest_bars[index], 0.0,
                       "HHVBARS clamps an oversized N to available history");
        require_number(extreme_bars, index, "L", lowest_bars[index], 0.0,
                       "LLVBARS clamps an oversized N to available history");
    }
    require_number(extreme_bars, 0, "HD", 0.0, 0.0,
                   "HHVBARS clamps an initially oversized dynamic N");
    require_number(extreme_bars, 0, "LD", 0.0, 0.0,
                   "LLVBARS clamps an initially oversized dynamic N");
    require_null(extreme_bars, 1, "HD",
                 "HHVBARS preserves a missing period bar");
    require_null(extreme_bars, 1, "LD",
                 "LLVBARS preserves a missing period bar");
    require_number(extreme_bars, 2, "HD", 1.0, 0.0,
                   "HHVBARS resumes after a missing period bar");
    require_number(extreme_bars, 2, "LD", 0.0, 0.0,
                   "LLVBARS resumes after a missing period bar");

    auto sentinel_extreme_bars_sample = sample(3);
    set_close_series(sentinel_extreme_bars_sample, {1.0, 10.0, 2.0});
    const auto sentinel_extreme_bars =
        tdx::evaluate_formula_source_document(
            sentinel_extreme_bars_sample,
            "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);"
            "H:HHVBARS(X,0);L:LLVBARS(X,0);",
            {}, "NATIVEEXTREMEBARSSENTINEL");
    const double sentinel_highest_bars[]{0.0, 1.0, 0.0};
    const double sentinel_lowest_bars[]{0.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(sentinel_extreme_bars, index, "H",
                       sentinel_highest_bars[index], 0.0,
                       "HHVBARS compares an internal native sentinel");
        require_number(sentinel_extreme_bars, index, "L",
                       sentinel_lowest_bars[index], 0.0,
                       "LLVBARS compares an internal native sentinel");
    }

    const auto tolerant_extreme_bars = tdx::evaluate_formula_source_document(
        sample(2),
        "A:=IF(CURRBARSCOUNT=2,100,99.99999);H:HHVBARS(A,2);"
        "B:=IF(CURRBARSCOUNT=2,100,100.00001);L:LLVBARS(B,2);",
        {}, "NATIVEEXTREMEBARSTOLERANCE");
    require_number(tolerant_extreme_bars, 1, "H", 0.0, 0.0,
                   "HHVBARS selects a later value inside native tolerance");
    require_number(tolerant_extreme_bars, 1, "L", 0.0, 0.0,
                   "LLVBARS selects a later value inside native tolerance");

    const auto native_bars_count = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT>=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE));B:BARSCOUNT(X);",
        {}, "NATIVEBARSCOUNT");
    require_null(native_bars_count, 0, "B",
                 "BARSCOUNT preserves its leading canonical sentinel");
    require_null(native_bars_count, 1, "B",
                 "BARSCOUNT waits for its first valid source bar");
    require_number(native_bars_count, 2, "B", 0.0, 0.0,
                   "BARSCOUNT numbers its first valid bar as zero");
    require_number(native_bars_count, 3, "B", 1.0, 0.0,
                   "BARSCOUNT advances across an internal missing source bar");
    require_number(native_bars_count, 4, "B", 2.0, 0.0,
                   "BARSCOUNT keeps advancing after an internal missing bar");

    const auto leading_backset = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT>=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=1,1,0));B:BACKSET(X,10);",
        {}, "NATIVEBACKSETLEADING");
    require_null(leading_backset, 0, "B",
                 "BACKSET preserves its leading native missing region");
    require_null(leading_backset, 1, "B",
                 "BACKSET keeps all bars before the first valid source missing");
    for (std::size_t index = 2; index < 5; ++index)
        require_number(leading_backset, index, "B", 1.0, 0.0,
                       "BACKSET clips an oversized backfill at the first valid bar");

    auto threshold_backset_sample = sample(6);
    set_close_series(threshold_backset_sample,
                     {0.00001, -0.00001, 0.0000099, -0.0000099,
                      0.0000101, -0.0000101});
    const auto threshold_backset = tdx::evaluate_formula_source_document(
        threshold_backset_sample, "B:BACKSET(CLOSE,1);", {},
        "NATIVEBACKSETTHRESHOLD");
    const double threshold_backset_values[]{0, 0, 0, 0, 1, 1};
    for (std::size_t index = 0; index < 6; ++index)
        require_number(threshold_backset, index, "B",
                       threshold_backset_values[index], 0.0,
                       "BACKSET uses a strict native float 1e-5 threshold");

    const auto dynamic_backset = tdx::evaluate_formula_source_document(
        sample(8),
        "X:=CURRBARSCOUNT=7 OR CURRBARSCOUNT=5 OR "
        "CURRBARSCOUNT=3 OR CURRBARSCOUNT=1;"
        "N:=IF(CURRBARSCOUNT=7,2.9,IF(CURRBARSCOUNT=5,-4,"
        "IF(CURRBARSCOUNT=3,DRAWNULL,"
        "IF(CURRBARSCOUNT=1,2147483648,99))));B:BACKSET(X,N);",
        {}, "NATIVEBACKSETDYNAMICPERIOD");
    const double dynamic_backset_values[]{1, 1, 0, 1, 0, 1, 0, 1};
    for (std::size_t index = 0; index < 8; ++index)
        require_number(dynamic_backset, index, "B",
                       dynamic_backset_values[index], 0.0,
                       "BACKSET truncates N through native float and clamps invalid N to one");

    const auto sentinel_backset = tdx::evaluate_formula_source_document(
        sample(4),
        "X:=IF(CURRBARSCOUNT=3,DRAWNULL,0);B:BACKSET(X,2);",
        {}, "NATIVEBACKSETSENTINEL");
    require_number(sentinel_backset, 0, "B", 1.0, 0.0,
                   "BACKSET lets a later native missing sentinel backfill history");
    require_number(sentinel_backset, 1, "B", 1.0, 0.0,
                   "BACKSET treats a post-start source sentinel as native true");
    require_number(sentinel_backset, 2, "B", 0.0, 0.0,
                   "BACKSET initializes post-start non-signals to zero");
    require_number(sentinel_backset, 3, "B", 0.0, 0.0,
                   "BACKSET retains zero after the sentinel backfill window");

    const auto native_filters = tdx::evaluate_formula_source_document(
        sample(3),
        "X:=IF(CURRBARSCOUNT=3,0.000005,"
        "IF(CURRBARSCOUNT=2,0.00001,1));F:FILTER(X,1);"
        "P:FILTER(1,0.99999999);"
        "Y:=CURRBARSCOUNT=3 OR CURRBARSCOUNT=1;R:FILTERX(Y,3);",
        {}, "NATIVEFILTERWINDOWS");
    const double filtered[]{0.0, 1.0, 0.0};
    const double float_period_filtered[]{1.0, 0.0, 1.0};
    const double reverse_oversized[]{1.0, 0.0, 1.0};
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_filters, index, "F", filtered[index], 0.0,
                       "FILTER uses the inclusive native boolean epsilon");
        require_number(native_filters, index, "P",
                       float_period_filtered[index], 0.0,
                       "FILTER converts N through native float before "
                       "integer truncation");
        require_number(native_filters, index, "R", reverse_oversized[index],
                       0.0,
                       "FILTERX leaves a prefix eligible when N exceeds cursor");
    }

    const auto native_count = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,"
        "IF(CURRBARSCOUNT=4,1,IF(CURRBARSCOUNT=3,0.999995,"
        "IF(CURRBARSCOUNT=2,1.00002,2))));"
        "C:COUNT(X,3);A:COUNT(X,0);"
        "P:=IF(CURRBARSCOUNT=5,1,IF(CURRBARSCOUNT=4,0,"
        "IF(CURRBARSCOUNT=3,2,IF(CURRBARSCOUNT=2,-1,3))));"
        "D:COUNT(X,P);",
        {}, "NATIVECOUNT");
    const double fixed_count[]{0, 1, 2, 2, 1};
    const double all_count[]{0, 1, 2, 2, 2};
    const double dynamic_count[]{0, 0, 2, 2, 1};
    for (std::size_t index = 0; index < 5; ++index) {
        require_number(native_count, index, "C", fixed_count[index], 0.0,
                       "COUNT counts only native values near exact one");
        require_number(native_count, index, "A", all_count[index], 0.0,
                       "constant COUNT zero selects all available history");
        require_number(native_count, index, "D", dynamic_count[index], 0.0,
                       "dynamic COUNT zero and negative periods stay distinct");
    }

    const auto count_epsilon = tdx::evaluate_formula_source_document(
        sample(4),
        "Q:=IF(CURRBARSCOUNT=4,-0.00001,0);E:COUNT(1,Q);"
        "Z:COUNT(1,0.00001);"
        "P:=IF(CURRBARSCOUNT=4,-0.00001,"
        "IF(CURRBARSCOUNT=3,-0.000011,"
        "IF(CURRBARSCOUNT=2,1,2)));D:COUNT(1,P);",
        {}, "NATIVECOUNTEPSILON");
    const double epsilon_constant_count[]{1, 2, 3, 4};
    const double epsilon_dynamic_count[]{0, 2, 1, 2};
    for (std::size_t index = 0; index < 4; ++index) {
        require_number(count_epsilon, index, "E",
                       epsilon_constant_count[index], 0.0,
                       "COUNT treats a period difference at epsilon as constant");
        require_number(count_epsilon, index, "Z", 0.0, 0.0,
                       "constant COUNT positive epsilon selects an empty window");
        require_number(count_epsilon, index, "D",
                       epsilon_dynamic_count[index], 0.0,
                       "dynamic COUNT distinguishes negative epsilon from below epsilon");
    }

    const auto native_every = tdx::evaluate_formula_source_document(
        sample(7),
        "X:=IF(CURRBARSCOUNT=7,DRAWNULL,IF(CURRBARSCOUNT=6,1,"
        "IF(CURRBARSCOUNT=5,2,IF(CURRBARSCOUNT=4,DRAWNULL,"
        "IF(CURRBARSCOUNT=3,0,1)))));"
        "P:=IF(CURRBARSCOUNT=2,DRAWNULL,2);E:EVERY(X,P);"
        "R:=IF(CURRBARSCOUNT=7,1,IF(CURRBARSCOUNT=6,2,"
        "IF(CURRBARSCOUNT=5,0,IF(CURRBARSCOUNT=4,2,"
        "IF(CURRBARSCOUNT=3,0,2)))));D:EVERY(1,R);"
        "M:=IF(CURRBARSCOUNT=3,DRAWNULL,2);K:EVERY(1,M);",
        {}, "NATIVEEVERY");
    require_null(native_every, 0, "E",
                 "EVERY preserves its leading source missing region");
    const double every_values[]{0, 1, 1, 0, 0, 0};
    for (std::size_t index = 1; index < 7; ++index)
        require_number(native_every, index, "E", every_values[index - 1], 0.0,
                       "EVERY uses the native consecutive-true state machine");
    const double retained_run[]{1, 1, 0, 1, 0, 1, 1};
    for (std::size_t index = 0; index < 7; ++index)
        require_number(native_every, index, "D", retained_run[index], 0.0,
                       "EVERY keeps its run length while N is below one");
    const double retained_missing_run[]{0, 1, 1, 1, 0, 1, 1};
    for (std::size_t index = 0; index < 7; ++index)
        require_number(native_every, index, "K", retained_missing_run[index],
                       0.0, "EVERY keeps a non-zero run while N is missing");

    const auto native_exist = tdx::evaluate_formula_source_document(
        sample(6),
        "X:=IF(CURRBARSCOUNT=6,DRAWNULL,IF(CURRBARSCOUNT=5,1,"
        "IF(CURRBARSCOUNT=4,0,IF(CURRBARSCOUNT=3,DRAWNULL,"
        "IF(CURRBARSCOUNT=2,0,1)))));"
        "N:=IF(CURRBARSCOUNT=1,3,IF(CURRBARSCOUNT=6,99,1));"
        "E:EXIST(X,N);",
        {}, "NATIVEEXIST");
    require_null(native_exist, 0, "E",
                 "EXIST preserves its leading missing region");
    const double exist_values[]{1, 1, 0, 1};
    require_number(native_exist, 1, "E", exist_values[0], 0.0,
                   "EXIST records a native true value");
    require_number(native_exist, 2, "E", exist_values[1], 0.0,
                   "EXIST retains a hit within the final-bar period");
    require_null(native_exist, 3, "E",
                 "EXIST leaves an internal source missing value missing");
    require_number(native_exist, 4, "E", exist_values[2], 0.0,
                   "EXIST expires a hit using the final-bar period only");
    require_number(native_exist, 5, "E", exist_values[3], 0.0,
                   "EXIST accepts a new hit after an internal missing value");

    const auto wrapped_exist = tdx::evaluate_formula_source_document(
        sample(3),
        "F:EXIST(0,-2147483648);"
        "H:EXIST(CURRBARSCOUNT=3,-2147483648);",
        {}, "NATIVEEXISTI32WRAP");
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(wrapped_exist, index, "F", 0.0, 0.0,
                       "EXIST preserves signed i32 wrap while seeding -N");
        require_number(wrapped_exist, index, "H", 1.0, 0.0,
                       "EXIST preserves signed i32 wrap while computing i-N");
    }

    const auto native_bars_last = tdx::evaluate_formula_source_document(
        sample(7),
        "X:=IF(CURRBARSCOUNT=7,DRAWNULL,IF(CURRBARSCOUNT=6,0,"
        "IF(CURRBARSCOUNT=5,1,IF(CURRBARSCOUNT=4,0,"
        "IF(CURRBARSCOUNT=3,DRAWNULL,IF(CURRBARSCOUNT=2,0,2))))));"
        "B:BARSLAST(X);C:BARSLASTCOUNT(X);",
        {}, "NATIVEBARSLAST");
    require_null(native_bars_last, 0, "B",
                 "BARSLAST skips a leading missing value");
    require_null(native_bars_last, 1, "B",
                 "BARSLAST skips a leading native zero");
    const double bars_last_values[]{0, 1, 0, 1, 0};
    for (std::size_t index = 2; index < 7; ++index)
        require_number(native_bars_last, index, "B",
                       bars_last_values[index - 2], 0.0,
                       "BARSLAST follows the native post-start counter");

    const auto native_bars_last_sentinel =
        tdx::evaluate_formula_source_document(
            sample(4),
            "X:=IF(CURRBARSCOUNT=4,-4.0398103E34,"
            "IF(CURRBARSCOUNT=3,0,IF(CURRBARSCOUNT=2,1,0)));"
            "B:BARSLAST(X);",
            {}, "NATIVEBARSLASTSENTINEL");
    require_null(native_bars_last_sentinel, 0, "B",
                 "BARSLAST skips a leading canonical sentinel");
    require_null(native_bars_last_sentinel, 1, "B",
                 "BARSLAST also skips a following native zero");
    require_number(native_bars_last_sentinel, 2, "B", 0.0, 0.0,
                   "BARSLAST starts at its first raw-float true value");
    require_number(native_bars_last_sentinel, 3, "B", 1.0, 0.0,
                   "BARSLAST counts from the first raw-float true value");
    require_null(native_bars_last, 0, "C",
                 "BARSLASTCOUNT preserves leading source missing");
    const double bars_last_count_values[]{0, 1, 0, 0, 0, 0};
    for (std::size_t index = 1; index < 7; ++index)
        require_number(native_bars_last, index, "C",
                       bars_last_count_values[index - 1], 0.0,
                       "BARSLASTCOUNT scans to zero and skips missing values");

    const auto native_bars_last_count =
        tdx::evaluate_formula_source_document(
            sample(5),
            "X:=IF(CURRBARSCOUNT=5,1,IF(CURRBARSCOUNT=4,2,"
            "IF(CURRBARSCOUNT=3,1.000005,IF(CURRBARSCOUNT=2,0,1))));"
            "C:BARSLASTCOUNT(X);",
            {}, "NATIVEBARSLASTCOUNT");
    const double count_since_zero[]{1, 1, 2, 0, 1};
    for (std::size_t index = 0; index < 5; ++index)
        require_number(native_bars_last_count, index, "C",
                       count_since_zero[index], 0.0,
                       "BARSLASTCOUNT counts only values near one");

    const auto native_bars_last_count_sentinel =
        tdx::evaluate_formula_source_document(
            sample(4),
            "X:=IF(CURRBARSCOUNT=4,-4.0398103E34,"
            "IF(CURRBARSCOUNT>=2,1,0));C:BARSLASTCOUNT(X);",
            {}, "NATIVEBARSLASTCOUNTSENTINEL");
    require_null(native_bars_last_count_sentinel, 0, "C",
                 "BARSLASTCOUNT skips a leading canonical sentinel");
    require_number(native_bars_last_count_sentinel, 1, "C", 1.0, 0.0,
                   "BARSLASTCOUNT starts at the first native source");
    require_number(native_bars_last_count_sentinel, 2, "C", 2.0, 0.0,
                   "BARSLASTCOUNT accumulates consecutive near-one values");
    require_number(native_bars_last_count_sentinel, 3, "C", 0.0, 0.0,
                   "BARSLASTCOUNT stops at a native zero");

    const auto native_not = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT=5 OR CURRBARSCOUNT=2,DRAWNULL,"
        "IF(CURRBARSCOUNT=4,0,IF(CURRBARSCOUNT=3,2,-1)));N:NOT(X);"
        "A:NOT(DRAWNULL);",
        {}, "NATIVENOT");
    require_null(native_not, 0, "N",
                 "NOT preserves only its leading missing region");
    const double expected_not[]{1.0, 0.0, 0.0, 0.0};
    for (std::size_t index = 1; index < 5; ++index)
        require_number(native_not, index, "N", expected_not[index - 1], 0.0,
                       "NOT uses exact zero after native evaluation starts");
    for (std::size_t index = 0; index < 5; ++index)
        require_null(native_not, index, "A",
                     "NOT leaves an all-missing input missing");

    const auto native_not_float_edges = tdx::evaluate_formula_source_document(
        sample(3),
        "Z:NOT(-0.0);P:NOT(1E-50);M:NOT(-1E-50);",
        {}, "NATIVENOTFLOAT");
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(native_not_float_edges, index, "Z", 1.0, 0.0,
                       "NOT treats negative zero as native float zero");
        require_number(native_not_float_edges, index, "P", 1.0, 0.0,
                       "NOT narrows a positive subnormal operand to float zero");
        require_number(native_not_float_edges, index, "M", 1.0, 0.0,
                       "NOT narrows a negative subnormal operand to float zero");
    }

    const auto ref_not = tdx::evaluate_formula_source_document(
        sample(4),
        "X:=IF(CURRBARSCOUNT=2,DRAWNULL,CLOSE);R:NOT(REF(X,1));",
        {}, "NATIVENOTREF");
    require_null(ref_not, 0, "R",
                 "NOT preserves the leading missing value introduced by REF");
    for (std::size_t index = 1; index < 4; ++index)
        require_number(ref_not, index, "R", 0.0, 0.0,
                       "NOT maps an internal REF missing value to native false");

    const auto bars_next_sub_epsilon =
        tdx::evaluate_formula_source_document(
            sample(3),
            "X:=IF(CURRBARSCOUNT=2,0.000005,0);B:BARSNEXT(X);",
            {}, "NATIVEBARSNEXTSUBEPSILON");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(bars_next_sub_epsilon, index, "B",
                     "BARSNEXT rejects a source strictly inside its float epsilon band");

    const auto bars_next_inclusive =
        tdx::evaluate_formula_source_document(
            sample(4),
            "X:=IF(CURRBARSCOUNT=4,-0.00001,"
            "IF(CURRBARSCOUNT=2,0.00001,0));B:BARSNEXT(X);",
            {}, "NATIVEBARSNEXTINCLUSIVE");
    const double bars_next_inclusive_values[]{0.0, 1.0, 0.0};
    for (std::size_t index = 0; index < 3; ++index)
        require_number(bars_next_inclusive, index, "B",
                       bars_next_inclusive_values[index], 0.0,
                       "BARSNEXT includes both native float epsilon endpoints");
    require_null(bars_next_inclusive, 3, "B",
                 "BARSNEXT leaves bars after its last true source missing");

    const auto bars_next_sentinel =
        tdx::evaluate_formula_source_document(
            sample(6),
            "X:=IF(CURRBARSCOUNT=6 OR CURRBARSCOUNT=3,1,"
            "IF(CURRBARSCOUNT=5,DRAWNULL,0));B:BARSNEXT(X);",
            {}, "NATIVEBARSNEXTSENTINEL");
    const double bars_next_sentinel_values[]{0.0, 2.0, 1.0, 0.0};
    for (std::size_t index = 0; index < 4; ++index)
        require_number(bars_next_sentinel, index, "B",
                       bars_next_sentinel_values[index], 0.0,
                       "BARSNEXT counts an internal sentinel while scanning to the next true bar");
    for (std::size_t index = 4; index < 6; ++index)
        require_null(bars_next_sentinel, index, "B",
                     "BARSNEXT preserves trailing missing output after its last true bar");

    const auto bars_next_all_false =
        tdx::evaluate_formula_source_document(
            sample(3),
            "X:=IF(CURRBARSCOUNT=3,DRAWNULL,"
            "IF(CURRBARSCOUNT=2,0.0000099,-0.0000099));B:BARSNEXT(X);",
            {}, "NATIVEBARSNEXTALLFALSE");
    for (std::size_t index = 0; index < 3; ++index)
        require_null(bars_next_all_false, index, "B",
                     "BARSNEXT leaves an all-sentinel-or-sub-epsilon source missing");

    auto zig_threshold_sample = sample(7);
    set_close_series(zig_threshold_sample,
                     {10.0, 11.0, 12.0, 11.5, 10.5, 9.0, 10.0});
    const auto zig_threshold = tdx::evaluate_formula_source_document(
        zig_threshold_sample,
        "P:ZIG(CLOSE,10);N:ZIG(CLOSE,-10);"
        "M:ZIG(CLOSE,DRAWNULL);O:ZIG(CLOSE,1E100);",
        {}, "NATIVEZIGTHRESHOLD");
    const double expected_positive_zig[]{10.0, 11.0, 12.0, 11.0,
                                         10.0, 9.0, 10.0};
    for (std::size_t index = 0; index < std::size(expected_positive_zig);
         ++index) {
        require_number(zig_threshold, index, "P", expected_positive_zig[index],
                       0.0, "ZIG follows the native positive percentage path");
        require_number(zig_threshold, index, "N", 0.0, 0.0,
                       "ZIG returns the native cleared vector for a negative threshold");
        require_number(zig_threshold, index, "M", 0.0, 0.0,
                       "ZIG safely keeps a missing threshold on the cleared vector");
        require_number(zig_threshold, index, "O", 0.0, 0.0,
                       "ZIG safely keeps float-overflow thresholds on the cleared vector");
    }

    auto zig_unconfirmed_extremum_sample = sample(5);
    set_close_series(zig_unconfirmed_extremum_sample,
                     {10.0, 11.0, 12.0, 11.5, 11.0});
    const auto zig_unconfirmed_extremum =
        tdx::evaluate_formula_source_document(
            zig_unconfirmed_extremum_sample, "Z:ZIG(CLOSE,20);", {},
            "NATIVEZIGUNCONFIRMED");
    const double expected_unconfirmed_extremum[]{10.0, 11.0, 12.0, 11.5, 11.0};
    for (std::size_t index = 0;
         index < std::size(expected_unconfirmed_extremum); ++index)
        require_number(
            zig_unconfirmed_extremum, index, "Z",
            expected_unconfirmed_extremum[index], 0.0,
            "ZIG preserves an older unconfirmed extremum before repainting the final leg");

    auto zig_tail_selector_sample = sample(12);
    set_close_series(zig_tail_selector_sample,
                     {10.0, 11.0, 12.0, 11.5, 10.5, 9.0,
                      10.0, 13.0, 12.0, 8.0, 9.0, 11.25});
    const auto zig_tail_selector = tdx::evaluate_formula_source_document(
        zig_tail_selector_sample,
        "X:=IF(CURRBARSCOUNT=12,99,3);"
        "T:ZIG(X,10);C:ZIG(CLOSE,10);",
        {}, "NATIVEZIGTAILSELECTOR");
    for (std::size_t index = 0; index < 12; ++index) {
        const auto expected = point_value(zig_tail_selector, index, "C").as_number();
        require_number(zig_tail_selector, index, "T", expected, 0.0,
                       "ZIG recognizes a selector from only its final ten adjacent pairs");
    }
    require_number(zig_tail_selector, 0, "T", 10.0, 0.0,
                   "ZIG ignores an older value outside the native selector tail window");

    auto zig_short_selector_sample = sample(3);
    set_close_series(zig_short_selector_sample, {20.0, 24.0, 18.0});
    const auto zig_short_selector = tdx::evaluate_formula_source_document(
        zig_short_selector_sample,
        "S:ZIG(3,10);C:ZIG(CLOSE,10);",
        {}, "NATIVEZIGSHORTSELECTOR");
    for (std::size_t index = 0; index < 3; ++index) {
        const auto expected = point_value(zig_short_selector, index, "C").as_number();
        require_number(zig_short_selector, index, "S", expected, 0.0,
                       "ZIG recognizes a constant selector in a short series");
    }

    auto zig_nonconstant_tail_sample = sample(12);
    set_close_series(zig_nonconstant_tail_sample,
                     {20.0, 20.0, 20.0, 20.0, 20.0, 20.0,
                      20.0, 20.0, 20.0, 20.0, 20.0, 20.0});
    const auto zig_nonconstant_tail = tdx::evaluate_formula_source_document(
        zig_nonconstant_tail_sample,
        "X:=IF(CURRBARSCOUNT=2,4,3);E:ZIG(X,100);",
        {}, "NATIVEZIGNONCONSTANTTAIL");
    for (std::size_t index = 0; index < 12; ++index)
        require_number(zig_nonconstant_tail, index, "E", 3.0, 0.0,
                       "ZIG keeps a non-constant selector tail as an explicit series");

    auto peak_endpoint_sample = sample(4);
    set_close_series(peak_endpoint_sample, {1.0, 2.0, 3.0, 4.0});
    const auto peak_endpoint = tdx::evaluate_formula_source_document(
        peak_endpoint_sample,
        "P:PEAK(CLOSE,1,1);B:PEAKBARS(CLOSE,1,1);",
        {}, "NATIVEPEAKENDPOINT");
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(peak_endpoint, index, "P", 0.0, 0.0,
                       "PEAK waits for the native initial endpoint candidate");
        require_number(peak_endpoint, index, "B", 0.0, 0.0,
                       "PEAKBARS waits for the native initial endpoint candidate");
    }
    require_number(peak_endpoint, 3, "P", 4.0, 0.0,
                   "PEAK exposes a rising final candidate without a strict three-point turn");
    require_number(peak_endpoint, 3, "B", 0.0, 0.0,
                   "PEAKBARS reports zero at the rising final candidate");

    auto trough_endpoint_sample = sample(4);
    set_close_series(trough_endpoint_sample, {4.0, 3.0, 2.0, 1.0});
    const auto trough_endpoint = tdx::evaluate_formula_source_document(
        trough_endpoint_sample,
        "T:TROUGH(CLOSE,1,1);B:TROUGHBARS(CLOSE,1,1);",
        {}, "NATIVETROUGHENDPOINT");
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(trough_endpoint, index, "T", 0.0, 0.0,
                       "TROUGH waits for the native initial endpoint candidate");
        require_number(trough_endpoint, index, "B", 0.0, 0.0,
                       "TROUGHBARS waits for the native initial endpoint candidate");
    }
    require_number(trough_endpoint, 3, "T", 1.0, 0.0,
                   "TROUGH exposes a falling final candidate without a strict three-point turn");
    require_number(trough_endpoint, 3, "B", 0.0, 0.0,
                   "TROUGHBARS reports zero at the falling final candidate");

    auto peak_order_sample = sample(5);
    set_close_series(peak_order_sample, {1.0, 3.0, 1.0, 3.0, 1.0});
    const auto peak_order = tdx::evaluate_formula_source_document(
        peak_order_sample,
        "O:=IF(CURRBARSCOUNT=1,1.99999999,1);"
        "P:PEAK(CLOSE,1,O);B:PEAKBARS(CLOSE,1,O);",
        {}, "NATIVEPEAKORDER");
    for (std::size_t index = 0; index < 3; ++index) {
        require_number(peak_order, index, "P", 0.0, 0.0,
                       "PEAK reads its f32 order only from the final bar");
        require_number(peak_order, index, "B", 0.0, 0.0,
                       "PEAKBARS reads its f32 order only from the final bar");
    }
    require_number(peak_order, 3, "P", 3.0, 0.0,
                   "PEAK retains the second latest native turn after queue rotation");
    require_number(peak_order, 4, "P", 3.0, 0.0,
                   "PEAK carries the selected native turn to the final bar");
    require_number(peak_order, 3, "B", 2.0, 0.0,
                   "PEAKBARS counts from the second latest native turn");
    require_number(peak_order, 4, "B", 3.0, 0.0,
                   "PEAKBARS advances the selected turn distance on the final bar");

    auto peak_float_sample = sample(3);
    set_close_series(peak_float_sample, {1.0, 2.0, 16777217.0});
    const auto peak_float = tdx::evaluate_formula_source_document(
        peak_float_sample, "P:PEAK(CLOSE,1,1);", {}, "NATIVEPEAKFLOAT");
    require_number(peak_float, 2, "P", 16777216.0, 0.0,
                   "PEAK returns the raw f32 value stored by the native ZIG buffer");

    const auto bars_since_float_edges = tdx::evaluate_formula_source_document(
        sample(4),
        "P:BARSSINCE(1E-50);N:BARSSINCE(-0.0);"
        "O:BARSSINCE(1E100);W:BARSSINCE(16777217);",
        {}, "NATIVEBARSSINCEFLOAT");
    for (std::size_t index = 0; index < 4; ++index) {
        require_null(bars_since_float_edges, index, "P",
                     "BARSSINCE skips values narrowed to positive float zero");
        require_null(bars_since_float_edges, index, "N",
                     "BARSSINCE skips exact native negative zero");
        require_null(bars_since_float_edges, index, "O",
                     "BARSSINCE maps float overflow to its canonical sentinel");
        require_number(bars_since_float_edges, index, "W",
                       static_cast<double>(index), 0.0,
                       "BARSSINCE starts on a non-zero raw float operand");
    }

    const auto bars_since_started = tdx::evaluate_formula_source_document(
        sample(5),
        "X:=IF(CURRBARSCOUNT=5,DRAWNULL,"
        "IF(CURRBARSCOUNT=4,1E-8,"
        "IF(CURRBARSCOUNT=3,DRAWNULL,0)));B:BARSSINCE(X);",
        {}, "NATIVEBARSSINCESTARTED");
    require_null(bars_since_started, 0, "B",
                 "BARSSINCE preserves its leading canonical sentinel");
    for (std::size_t index = 1; index < 5; ++index)
        require_number(bars_since_started, index, "B",
                       static_cast<double>(index - 1), 0.0,
                       "BARSSINCE starts on a representable tiny float and never resets");
}

}  // namespace formula_engine_test
