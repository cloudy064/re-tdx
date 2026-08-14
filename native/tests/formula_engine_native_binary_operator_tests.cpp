#include "formula_engine_test_support.hpp"

namespace formula_engine_test {

void run_native_binary_operator_tests() {
    const auto literals = tdx::evaluate_formula_source_document(
        sample(1), "I:16777217;D:0.1;O:1E100;", {},
        "NATIVELITERALFLOAT");
    require(point_value(literals, 0, "I").is_number() &&
                point_value(literals, 0, "I").as_number() == 16777216.0,
            "numeric integer literals land in raw float before broadcast");
    require(point_value(literals, 0, "D").is_number() &&
                point_value(literals, 0, "D").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)),
            "numeric decimal literals land in raw float before broadcast");
    require(point_value(literals, 0, "O").is_null(),
            "an out-of-range numeric literal is contained as nonfinite output");

    const auto parameter = tdx::evaluate_formula_source_document(
        sample(1), "P:N;Q:N+0;", {{"N", 16777217.0}},
        "NATIVEPARAMETERFLOAT");
    require(point_value(parameter, 0, "P").is_number() &&
                point_value(parameter, 0, "P").as_number() == 16777216.0 &&
                point_value(parameter, 0, "Q").is_number() &&
                point_value(parameter, 0, "Q").as_number() == 16777216.0 &&
                parameter.at("parameters").at("N").as_number() == 16777216.0,
            "formula parameters use and report their native raw-f32 value");
    bool parameter_overflow_rejected = false;
    try {
        (void)tdx::evaluate_formula_source_document(
            sample(1), "P:N;", {{"N", 1E100}},
            "NATIVEPARAMETERFLOATOVERFLOW");
    } catch (const tdx::Error&) {
        parameter_overflow_rejected = true;
    }
    require(parameter_overflow_rejected,
            "formula parameters reject values outside native float range");

    auto field_sample = sample(1);
    auto& field_bar = field_sample["bars"].as_array()[0];
    field_bar["open"] = 16777217.0;
    field_bar["high"] = 0.1;
    field_bar["low"] = -16777217.0;
    field_bar["close"] = 16777219.0;
    field_bar["amount"] = 16777217.0;
    field_bar["volume"] = 16777217.0;
    const auto fields = tdx::evaluate_formula_source_document(
        field_sample, "O:OPEN;H:HIGH;L:LOW;C:CLOSE;A:AMOUNT;V:VOL;",
        {}, "NATIVEPRICEFIELDFLOAT");
    require(point_value(fields, 0, "O").as_number() == 16777216.0 &&
                point_value(fields, 0, "H").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)) &&
                point_value(fields, 0, "L").as_number() == -16777216.0 &&
                point_value(fields, 0, "C").as_number() == 16777220.0 &&
                point_value(fields, 0, "A").as_number() == 16777216.0,
            "core K-line formula fields expose their native raw-f32 values");
    const auto expected_volume = static_cast<double>(static_cast<float>(
        static_cast<double>(static_cast<float>(16777217.0)) / 100.0));
    require(point_value(fields, 0, "V").as_number() == expected_volume,
            "formula volume narrows the native source before unit scaling");
    const auto& field_point = fields.at("points").as_array().at(0);
    require(field_point.at("open").as_number() == 16777217.0 &&
                field_point.at("high").as_number() == 0.1 &&
                field_point.at("low").as_number() == -16777217.0 &&
                field_point.at("close").as_number() == 16777219.0 &&
                field_point.at("amount").as_number() == 16777217.0 &&
                field_point.at("volume").as_number() == 16777217.0,
            "source K-line point fields remain the caller-owned document values");

    auto auxiliary_sample = sample(1);
    auto& auxiliary_bar = auxiliary_sample["bars"].as_array()[0];
    auxiliary_bar["open_interest"] = 16777217.0;
    auxiliary_bar["hk_short_volume"] = 0.1;
    auxiliary_bar["auxiliary_price"] = -16777217.0;
    const auto auxiliary = tdx::evaluate_formula_source_document(
        auxiliary_sample,
        "I:VOLINSTK;K:HKSHORTVOL;Z:ZSTJJ;Q:QHJSJ;", {},
        "NATIVEAUXILIARYFIELDFLOAT");
    require(point_value(auxiliary, 0, "I").as_number() == 16777216.0 &&
                point_value(auxiliary, 0, "K").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)) &&
                point_value(auxiliary, 0, "Z").as_number() == -16777216.0 &&
                point_value(auxiliary, 0, "Q").as_number() == -16777216.0,
            "auxiliary K-line formula fields expose native raw-f32 values");
    const auto& auxiliary_point = auxiliary.at("points").as_array().at(0);
    require(auxiliary_point.at("open_interest").as_number() == 16777217.0 &&
                auxiliary_point.at("hk_short_volume").as_number() == 0.1,
            "auxiliary point metadata remains the caller-owned document value");

    tdx::Json scalar_context = tdx::Json::object();
    scalar_context["symbols"] = tdx::Json::object();
    scalar_context["symbols"]["CAPITAL"] = 16777217.0;
    scalar_context["symbols"]["TOTALCAPITAL"] = -16777217.0;
    scalar_context["symbols"]["MULTIPLIER"] = 0.1;
    scalar_context["formula_scalar_bindings"] = tdx::Json::object();
    scalar_context["formula_scalar_bindings"]["MINDIFF"] = 0.1;
    const auto scalar_context_result = tdx::evaluate_formula_source_document(
        sample(1), "C:CAPITAL;T:TOTALCAPITAL;D:MINDIFF;M:MULTIPLIER;", {},
        "NATIVESCALARCONTEXTFLOAT", &scalar_context);
    require(point_value(scalar_context_result, 0, "C").as_number() == 16777216.0 &&
                point_value(scalar_context_result, 0, "T").as_number() == -16777216.0 &&
                point_value(scalar_context_result, 0, "D").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)) &&
                point_value(scalar_context_result, 0, "M").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)),
            "dedicated scalar context handlers expose native raw-f32 values");

    auto scalar_series_sample = sample(2);
    tdx::Json scalar_series_context = tdx::Json::object();
    scalar_series_context["series"] = tdx::Json::object();
    scalar_series_context["series"]["CAPITAL"] = tdx::Json::object();
    scalar_series_context["series"]["CAPITAL"]["2026-01-02|15:00"] = 16777217.0;
    scalar_series_context["series"]["CAPITAL"]["2026-01-01|15:00"] = 0.1;
    const auto scalar_series_result = tdx::evaluate_formula_source_document(
        std::move(scalar_series_sample), "C:CAPITAL;", {},
        "NATIVESCALARCONTEXTSERIESFLOAT", &scalar_series_context);
    require(point_value(scalar_series_result, 0, "C").as_number() ==
                    static_cast<double>(static_cast<float>(0.1)) &&
                point_value(scalar_series_result, 1, "C").as_number() == 16777216.0,
            "dedicated scalar context series narrow each point to raw float32");

    auto automatic_mindiff_sample = sample(1);
    automatic_mindiff_sample["min_tick"] = 0.1;
    const auto automatic_mindiff = tdx::evaluate_formula_source_document(
        std::move(automatic_mindiff_sample), "D:MINDIFF;", {},
        "NATIVEMINDIFFFLOAT");
    require(point_value(automatic_mindiff, 0, "D").as_number() ==
                static_cast<double>(static_cast<float>(0.1)),
            "automatic MINDIFF reads and broadcasts the native float32 tick");

    tdx::Json numbered_context = tdx::Json::object();
    numbered_context["finance"] = tdx::Json::object();
    numbered_context["finance"]["3"] = 3.25;
    numbered_context["finance"]["7"] = 16777217.0;
    numbered_context["dynainfo"] = tdx::Json::object();
    numbered_context["dynainfo"]["3"] = 0.1;
    numbered_context["dynainfo"]["4"] = -16777217.0;
    numbered_context["finvalue"] = tdx::Json::object();
    numbered_context["finvalue"]["308"] = 0.1;
    numbered_context["finvalue"]["309"] = 16777219.0;
    const auto numbered = tdx::evaluate_formula_source_document(
        sample(2),
        "F:FINANCE(IF(ISLASTBAR,7,3));"
        "D:DYNAINFO(IF(ISLASTBAR,4,3));"
        "V:FINVALUE(IF(ISLASTBAR,309,308));",
        {}, "NATIVENUMBEREDCONTEXTFLOAT", &numbered_context);
    for (std::size_t index = 0; index < 2; ++index) {
        require(point_value(numbered, index, "F").as_number() == 16777216.0 &&
                    point_value(numbered, index, "D").as_number() == -16777216.0 &&
                    point_value(numbered, index, "V").as_number() == 16777220.0,
                "numbered contexts use the final raw-f32 selector and output buffer");
    }

    auto professional_sample = sample(2);
    tdx::Json professional_context = tdx::Json::object();
    professional_context["series"] = tdx::Json::object();
    const auto add_professional_series = [&](const char* key, double value) {
        professional_context["series"][key] = tdx::Json::object();
        for (const auto& bar : professional_sample.at("bars").as_array()) {
            const auto point_key = bar.at("date").as_string() + "|" +
                bar.at("time").as_string();
            professional_context["series"][key][point_key] = value;
        }
    };
    add_professional_series("GPJYVALUE#6#1#1", 16777217.0);
    add_professional_series("BKJYVALUE#5#2#1", -16777217.0);
    add_professional_series("SCJYVALUE#2#2#0", 0.1);
    add_professional_series("GPJYVALUE#5#2#2", 3.25);
    add_professional_series("BKJYVALUE#6#1#2", 4.25);
    add_professional_series("SCJYVALUE#31#1#1", 5.25);
    const auto professional = tdx::evaluate_formula_source_document(
        std::move(professional_sample),
        "G:GPJYVALUE(IF(ISLASTBAR,6,5),IF(ISLASTBAR,1,2),"
        "IF(ISLASTBAR,1,2));"
        "B:BKJYVALUE(IF(ISLASTBAR,5,6),IF(ISLASTBAR,2,1),"
        "IF(ISLASTBAR,1,2));"
        "S:SCJYVALUE(IF(ISLASTBAR,2,31),IF(ISLASTBAR,2,1),"
        "IF(ISLASTBAR,0,1));",
        {}, "NATIVEPROFESSIONALCONTEXTFLOAT", &professional_context);
    for (std::size_t index = 0; index < 2; ++index) {
        require(point_value(professional, index, "G").as_number() ==
                        16777216.0 &&
                    point_value(professional, index, "B").as_number() ==
                        -16777216.0 &&
                    point_value(professional, index, "S").as_number() ==
                        static_cast<double>(static_cast<float>(0.1)),
                "professional contexts use three final raw-f32 selectors and f32 output");
    }

    auto add_sample = sample(2);
    auto& add_bars = add_sample["bars"].as_array();
    add_bars[1]["close"] = 16777217.0;
    add_bars[0]["close"] = 20.0;
    const auto add = tdx::evaluate_formula_source_document(
        add_sample,
        "A:CLOSE+1;B:0.1+0.2;L:DRAWNULL+1;R:1+DRAWNULL;"
        "O:1E100+1;",
        {}, "NATIVEADDRAWFLOAT");
    require(point_value(add, 0, "A").is_number() &&
                point_value(add, 0, "A").as_number() == 16777216.0,
            "addition narrows both inputs and output through raw float");
    require(point_value(add, 0, "B").is_number() &&
                point_value(add, 0, "B").as_number() ==
                    static_cast<double>(static_cast<float>(
                        static_cast<float>(0.1) + static_cast<float>(0.2))),
            "addition preserves the native float landing point");
    require(point_value(add, 0, "L").is_null() &&
                point_value(add, 0, "R").is_null(),
            "addition propagates either native missing operand");
    require(point_value(add, 0, "O").is_null(),
            "addition safely maps float overflow to missing");

    auto negate_sample = sample(1);
    negate_sample["bars"].as_array()[0]["close"] = 16777217.0;
    const auto negate = tdx::evaluate_formula_source_document(
        negate_sample,
        "C:-CLOSE;F:-0.1;M:-DRAWNULL;O:-1E100;",
        {}, "NATIVEUNARYNEGATEFLOAT");
    require(point_value(negate, 0, "C").is_number() &&
                point_value(negate, 0, "C").as_number() == -16777216.0,
            "unary minus lowers through the raw-f32 multiply primitive");
    require(point_value(negate, 0, "F").is_number() &&
                point_value(negate, 0, "F").as_number() ==
                    static_cast<double>(static_cast<float>(-0.1)),
            "unary minus preserves the native float landing point");
    require(point_value(negate, 0, "M").is_null() &&
                point_value(negate, 0, "O").is_null(),
            "unary minus propagates missing and float overflow safely");

    const auto equality = tdx::evaluate_formula_source_document(
        sample(1),
        "F:16777217=16777216;I:0=0.0000099;B:0=0.00001;"
        "N:0<>0.00001;J:0<>-0.00001;"
        "MM:DRAWNULL=DRAWNULL;MV:DRAWNULL=1;"
        "NM:DRAWNULL<>DRAWNULL;NV:DRAWNULL<>1;",
        {}, "NATIVEEQUALITYFLOAT");
    const auto number = [&](const char* name, double expected,
                            const char* message) {
        const auto& value = point_value(equality, 0, name);
        require(value.is_number() && value.as_number() == expected, message);
    };
    number("F", 1.0, "equality compares raw-f32 operands");
    number("I", 1.0, "equality accepts a difference inside native epsilon");
    number("B", 0.0, "equality excludes the positive epsilon boundary");
    number("N", 1.0, "not-equal includes the positive epsilon boundary");
    number("J", 1.0, "not-equal includes the negative epsilon boundary");
    number("MM", 1.0, "two canonical missing sentinels compare equal");
    number("MV", 0.0, "a missing sentinel does not equal an ordinary value");
    number("NM", 0.0, "two canonical missing sentinels are not unequal");
    number("NV", 1.0, "missing and ordinary values compare unequal");
}

}  // namespace formula_engine_test
