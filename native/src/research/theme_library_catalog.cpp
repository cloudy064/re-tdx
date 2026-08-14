#include "tdx/theme_library.hpp"

namespace tdx {

// The five ZTTZ masters the client ships. These are independent source
// snapshots rather than a hierarchy, so ids may repeat across them and
// record_id is always SOURCE:ID.
const std::vector<ThemeLibrarySource>& theme_library_sources() {
    static const std::vector<ThemeLibrarySource> sources{
        {"region", "区域经济", "list/func_zttz102_1.jsn"},
        {"state-owned", "国企系", "list/func_zttz103_1.jsn"},
        {"company", "公司系", "list/func_zttz104_1.jsn"},
        {"general", "统一主题", "list/func_zttz105_1.jsn"},
        {"holdings", "参股持股", "list/func_zttz106_1.jsn"},
    };
    return sources;
}

}  // namespace tdx
