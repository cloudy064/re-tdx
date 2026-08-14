/**
 * 全局导航模型 —— 信息架构的唯一定义。
 *
 * 重构前：5 个顶级 tab + 数据中心 13 个横向滚动药丸 + 个股抽屉 19 个平铺 tab，
 * 三层全是无分组的扁平列表。这里把它们收敛成「区 → 组 → 项」三级树，
 * NavRail、路由和 Omnibox 全部从这份模型派生，不再各写一份。
 */

export type IconName =
  | 'market'
  | 'sectors'
  | 'data'
  | 'protocol'
  | 'system';

export interface NavItem {
  /** 路由路径，形如 `/data/ownership` */
  path: string;
  label: string;
  /** 一行说明，出现在 Omnibox 结果和折叠态 tooltip */
  hint: string;
}

export interface NavGroup {
  label: string;
  items: NavItem[];
}

export interface NavSection {
  id: string;
  label: string;
  icon: IconName;
  groups: NavGroup[];
}

export const NAV: NavSection[] = [
  {
    id: 'market',
    label: '行情',
    icon: 'market',
    groups: [
      {
        label: '',
        items: [
          { path: '/stock', label: '个股工作台', hint: 'K 线 · 盘口 · 竞价 · 逐笔 · 全部个股数据' },
          { path: '/ranking', label: '实时榜单', hint: '涨速 · 涨幅 · 封单额 · 开盘抢筹等九种服务端排序' }
        ]
      }
    ]
  },
  {
    id: 'sectors',
    label: '板块',
    icon: 'sectors',
    groups: [
      {
        label: '',
        items: [
          { path: '/sectors', label: '板块浏览器', hint: '行业 · 概念 · 风格 · 指数板块与成分股' },
          { path: '/data/limit-ladder', label: '连板天梯', hint: '行业 · 概念封板、炸板、连板高度与晋级率验证' },
          { path: '/data/block-rotation', label: '板块轮动', hint: '行业 · 概念 · 地区 · 风格多周期异动与区间涨幅' },
          { path: '/data/block-backtest', label: '板块历史回测', hint: '五类板块区间表现 · 服务端选定成员 · 非历史时点成分重建' },
          { path: '/data/strategic-themes', label: '战略主题', hint: '24 个大类 · 567 个主题 · 成分股 · 逐股入选逻辑' },
          { path: '/data/theme-library', label: '统一主题库', hint: '850 个统一主题 · 五类来源 · 纳入原因 · 主题指数' },
          { path: '/data/thematic-opportunities', label: '主题机会', hint: '行业 / 区域 / 遗留主题 · 逐股逻辑 · 已爆炒 / 正爆炒复盘' },
          { path: '/industry', label: '行业机构画像', hint: '一级行业机构持仓与二级行业股东结构' }
        ]
      }
    ]
  },
  {
    id: 'data',
    label: '数据',
    icon: 'data',
    groups: [
      {
        label: '资金与交易',
        items: [
          { path: '/data/limit-quality', label: '涨停质量', hint: '封单 · 封流比 · 封昨比 · 竞价抢筹四路验证' },
          { path: '/data/limit-review', label: '涨跌停复盘', hint: '盘后逐步更新 · 当日原因 · 市场历史 · 指定日成员 · 年度行为' },
          { path: '/data/session-turnover', label: '开盘与盘后成交', hint: 'A 股 / ETF · 开盘成交 · 盘后成交 · 成交占比与涨跌幅' },
          { path: '/data/margin', label: '融资融券', hint: '市场总量 · 证券排行 · 单票近三月两融历史' },
          { path: '/data/stock-connect', label: '沪深港通', hint: '日周资金 · 当前与季度持仓 · 单票持股历史' },
          { path: '/data/intelligence', label: '市场情报', hint: '关注度 · 安全亮点 · 风险观察 · 事件股票关系' },
          { path: '/data/capital-strength', label: 'DDX资金强势', hint: '5日 / 10日 / 20日 / 30日 / 近3月前百 · 跨周期共振' },
          { path: '/data/strong-stocks', label: '强势股生命周期', hint: '历史连板区间 · 区间收益 · 逐日涨停原因 · 市场温度' },
          { path: '/data/commodity-links', label: '商股联动', hint: '商品多周期报价 · 涨价题材 · 驱动事件 · 关联股票与行业 ETF' },
          { path: '/data/announcement-signals', label: '公告精选', hint: '公告多空 · 风险提示 · PDF 原文 · 单票公告前后表现' },
          { path: '/data/corporate-orders', label: '公司订单', hint: '招投标 · 中标 · 重大合同 · 金额与营收占比 · 公告原文' },
          { path: '/data/exchange-supervision', label: '交易监管', hint: '当前监管观察期 · 历史记录 · 区间表现 · 异动公告 PDF' },
          { path: '/data/reverse-repo', label: '国债逆回购', hint: '实时年化 · 计息天数 · 手续费 · 净收益 · 资金可用/可取日' },
          { path: '/data/threshold-stocks', label: '千亿百元股', hint: '百元股 · 千亿市值 · 历史家数 · 日期名单 · 入围跌出' },
          { path: '/data/futures-issuance', label: '期货与发行', hint: '期货 · IPO/债券 · 定向增发全生命周期' },
          { path: '/data/expansion-market', label: '扩展市场行情', hint: '期货与商品指数 · 合约目录 · 五档 · 分时 · 逐笔开平' },
          { path: '/data/options', label: '期权工作台', hint: '商品与股指期权目录 · 到期规则 · IV · Greeks · 持仓结构' },
          { path: '/data/local-reference', label: '本地参考档案', hint: '历史证券兼容名 · 基金标的映射 · 指数图事件 · 零网络' },
          { path: '/data/block-trades', label: '大宗交易', hint: '成交 · 意向申报 · 营业部排行 · 行业分布' },
          { path: '/data/institution-lhb', label: '机构龙虎', hint: '一周 / 一月 / 三月 / 一年机构席位买卖排行' },
          { path: '/data/active-lhb', label: '活跃龙虎', hint: '近5日 / 一月 / 半年上榜次数 · 累计买卖额 · 历史异动' },
          { path: '/data/foreign-alerts', label: '外资预警', hint: '外资持股占比预警与状态历史' },
          { path: '/data/active-funds', label: '主动基金', hint: '基金季报重仓股汇总与持有基金明细' },
          { path: '/data/etf-flows', label: 'ETF 资金', hint: 'ETF 持股、申赎资金及一级行业资金动向' },
          { path: '/data/securities', label: '证券目录', hint: '行情主站股票、指数、ETF、债券与价格精度主表' },
          { path: '/data/convertible-bonds', label: '可转债定价', hint: 'L1 行情、转股溢价、YTM、纯债、双低及条款' },
          { path: '/data/bond-reference', label: '债券条款', hint: '信用评级 · 利率类型 · 债券类别 · 完整付息序列' },
          { path: '/data/economic-indicators', label: '经济指标', hint: '价格 / 景气指标 · 环比同比 · 历史走势 · 相关股票' },
          { path: '/data/calendar', label: '财经日历', hint: '宏观会议 · 公司重大事项 · 北证申购详情 · 美股 IPO 阶段 · 板块资讯' },
          { path: '/data/event-impact', label: '事件指数冲击', hint: '重大事件 · 上证/恒生/纳指 · 事件前后窗口表现' },
          { path: '/data/overview-factors', label: '大盘影响因素', hint: '央行 · 资金 · 估值 · 股债 · 海外市场等 17 项客户端判断快照' },
          { path: '/data/hk-events', label: '港股事件', hint: '分红派息 · 权益披露 · 沽空统计 · 上市申请' },
          { path: '/data/hk-reference', label: '港股本地档案', hint: '历史公司行动 · 本地财务快照 · 原生复权因子 · 零网络' },
          { path: '/data/special-situations', label: '并购、转板与预警', hint: '并购重组四阶段 · 新三板转板监管 · 合并定价 · 市值阈值' },
          { path: '/data/special-attention', label: '特别关注', hint: '股权分散 · *ST风险 · 摘星摘帽 · 立案调查 · 商誉风险' },
          { path: '/data/exchange-funds', label: '场内基金', hint: 'ETF多周期表现 · 货币ETF套利与收益 · REITs发行项目' },
          { path: '/data/fund-statistics', label: '基金统计', hint: '新发与上市基金 · 分红 · 股票基金收益 · 基金/ETF规模与申赎' },
          { path: '/data/fund-calendar', label: '基金事件日历', hint: '开放与分红事件 · 日期 · 内容 · 基金类别' },
          { path: '/data/specialized-metrics', label: '金融专项指标', hint: '银行资本与息差 · 券商月度经营 · 保险偿付与投资结构' },
          { path: '/data/company-changes', label: '公司变更库', hint: '证券与公司更名 · 指数调整 · 股权转让 · 实控人与行业变更' },
          { path: '/data/financial-screen', label: '财务筛选', hint: '五板横截面 · 估值 · 成长 · ROE与毛利 · 机构持仓 · 股息率' },
          { path: '/data/financial-insights', label: '特色财务线索', hint: '质量 · 利润预警 · 现金流 · 业绩反转 · 投资风险' },
          { path: '/data/gdr', label: 'GDR跨市场', hint: 'GDR行情 · A股映射 · 兑换比例 · 折算价与溢价率' },
          { path: '/data/equity-performance', label: 'A股多周期表现', hint: '全A股 · 5/20/60日 · 本月与年内涨幅 · 成交额' },
          { path: '/data/global-performance', label: '全球表现', hint: '主要指数 · 海外中资证券 · 5/20/60日 · 本月与年内表现' },
          { path: '/data/shareholder-signals', label: '牛散机构信号', hint: '股票反查信号 · 知名自然人目录与逐人持仓 · 机构增持 · 调研成长' },
          { path: '/data/recent-watch', label: '近期关注', hint: '绩价背离 · 涉外经营与人民币波动 · ST业绩预盈' },
          { path: '/data/patent-statistics', label: '公司专利', hint: '本期申请 · 本期授权 · 累计发明/实用/外观专利 · 深沪京公司' },
          { path: '/data/benchmark-analysis', label: '相对基准分析', hint: '十三段牛熊周期 · 个股与行业超额 · 停复牌 · 次新股表现' },
          { path: '/data/curated-data', label: '客户端精选', hint: '低估值 · 高分红 · 破净国企 · 转融券风险 · 传媒娱乐 · 港股表现' }
        ]
      },
      {
        label: '股东与股权',
        items: [
          { path: '/data/state-owned-reform', label: '国企改革', hint: '行业 / 地区 / 整合预期 / 公司系 · 控制人 · 重组预期' },
          { path: '/data/ownership', label: '股权变动', hint: '增减持 · 董监高 · 承诺不减持 · 质押风险' },
          { path: '/data/repurchases', label: '股份回购', hint: 'A 股回购方案 · 月度进度 · 港股逐笔回购' },
          { path: '/data/tender-offers', label: '要约收购', hint: '收购进度 · 要约价格 · 拟定与实际规模 · 期限与目的' },
          { path: '/data/unlocks', label: '限售解禁', hint: '解禁日历与逐股东批次明细' }
        ]
      },
      {
        label: '预期与研究',
        items: [
          { path: '/data/flow-followup', label: '资金信号后续表现', hint: '两融 / 北向逐日历史 · 六类视图 · 沪深300后续表现 · 占位审计' },
          { path: '/data/hot-history', label: 'K线历史热点', hint: '本地多年热点区间 · 主题 · 区间与峰值收益 · 完整分析' },
          { path: '/data/anomaly-risk', label: '交易异动与风险', hint: '十九类异动 · 上榜原因 · 停牌风险 · 利润断层' },
          { path: '/data/fund-analytics', label: '基金风险分析', hint: '收益风险 · 择时选股 · 公开持仓 · 稳定性 · 仓位估算' },
          { path: '/data/factor-signals', label: '因子与技术信号', hint: '因子目录 · 成分股关联 · 因子看板 · 三十二类技术选股' },
          { path: '/data/consensus', label: '一致预期', hint: '评级 · 复合增速 · 年内高低点 · 连涨' },
          { path: '/data/forecasts', label: '业绩预告', hint: '行业与报告期维度的业绩预告统计' },
          { path: '/data/research', label: '机构调研', hint: '调研 · 互动问答 · 行业热度 · 监管问询' },
          { path: '/data/roadshows', label: '上市公司路演', hint: '业绩说明会 · 发布会 · IPO与重大事项路演 · 单票历史' },
          { path: '/data/institution-analysis', label: '机构持仓全景', hint: '机构分类 · 社保汇总 · 基金独门 · 汇金证金 · 浮筹结构 · 单票反查' },
          { path: '/data/employees', label: '员工与持股', hint: '员工规模、薪酬、研发、高管名单与员工持股计划' },
          { path: '/data/ratings', label: '评级雷达', hint: '港股目标价与一级行业看多看空统计' }
        ]
      },
      {
        label: '估值',
        items: [
          { path: '/data/valuation-research', label: '估值与指数研究', hint: '相对估值 · 个股/行业 PE/PB-ROE · 波动率 · 全收益差' },
          { path: '/data/valuation', label: '市场估值', hint: '指数 PE/PB 历史与百分位、关联 ETF' }
        ]
      }
    ]
  },
  {
    id: 'protocol',
    label: '协议',
    icon: 'protocol',
    groups: [
      {
        label: '',
        items: [
          { path: '/protocol/jsn', label: 'JSN 资源目录', hint: '已编目的静态资源主表与下载状态' },
          { path: '/protocol/tqlex', label: 'TQLEX 云查询', hint: 'reqformat=2 模板、入口与动态参数' },
          { path: '/protocol/pbrpc', label: 'PBRPC 策略查询', hint: 'reqformat=22 模板与 protobuf 外层编码' },
          { path: '/protocol/routes', label: '旧云路由诊断', hint: 'reqformat=1 服务名、跨格式关联与 TPData 宿主边界' },
          { path: '/protocol/workflows', label: '关联工作流', hint: 'TQLEX/PBRPC 固定主表—明细协议链' },
          { path: '/protocol/coverage', label: '协议覆盖审计', hint: '云语义变体与 JSN 类型化命令缺口' },
          { path: '/protocol/formulas', label: '公式库', hint: '内置公式目录、纯 C++ 指标计算与 TPool 诊断' },
          { path: '/protocol/pools', label: 'TPool 股票池', hint: 'XML 流程、规则过滤、跨证券排名与只读流程投影' },
          { path: '/protocol/level2', label: 'Level2 实验室', hint: '合法会话预检、离线解码器与权限边界' }
        ]
      }
    ]
  },
  {
    id: 'system',
    label: '系统',
    icon: 'system',
    groups: [
      {
        label: '',
        items: [
          { path: '/system', label: '服务状态', hint: '后端健康、接口清单与安装目录盘点' }
        ]
      }
    ]
  }
];

/** 扁平化后的全部导航项，供 Omnibox 搜索与路由回填标题使用。 */
export const NAV_ITEMS: Array<NavItem & { section: string }> = NAV.flatMap((section) =>
  section.groups.flatMap((group) =>
    group.items.map((item) => ({ ...item, section: section.label }))
  )
);

/** 由路径反查导航项；未命中返回 null（例如 /stock/sz/000001 这类带参数的路径）。 */
export function findNavItem(path: string): (NavItem & { section: string }) | null {
  return NAV_ITEMS.find((item) => item.path === path) ?? null;
}

/** 路径属于哪个区，用于 NavRail 高亮。 */
export function sectionOf(path: string): string {
  const direct = NAV.find((section) =>
    section.groups.some((group) => group.items.some((item) => path.startsWith(item.path)))
  );
  return direct?.id ?? 'market';
}
