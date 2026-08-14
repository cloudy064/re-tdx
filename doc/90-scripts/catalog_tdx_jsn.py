#!/usr/bin/env python3
"""Build a field and coverage catalog for downloaded TDX JSN resources."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import Sequence

import download_tdx_jsn as downloader
import inventory_tdx_cloud_features as cloud
import update_tdx_blocks as updater


RESOURCE_DESCRIPTIONS = {
    "func_aqfph101_1.jsn": "安全评分、风险雷点与评分变动",
    "func_ygxc101_1.jsn": "员工、薪酬、研发与学历结构",
    "dfkzz201_1.jsn": "待发行可转债与申购安排",
    "func_gx_cbsj101_1.jsn": "财务、估值、成长、周转与机构持仓",
    "func_gx_cbyg101_1.jsn": "业绩预告区间与同比变化",
    "func_gx_fxgz101_1.jsn": "股权质押、商誉和限售解禁风险",
    "func_gx_fxspj101_1.jsn": "分析师评级、目标价与盈利预测",
    "func_gx_hyzt101_1.jsn": "通达信行业、主题及其证券成员",
    "func_gx_lhbd101_1.jsn": "龙虎榜历史、席位和净买入",
    "func_gx_rzrq101_1.jsn": "融资融券余额、流量和占比",
    "func_gx_zcjc101_1.jsn": "股东增减持数量、价格和区间",
    "func_gx_zfsp101_1.jsn": "分红送转、配股和高送转潜力",
    "func_gx_zjlx101_1.jsn": "1/5/10/20/30 日资金净流入",
    "func_gznhg100_1.jsn": "国债逆回购期限、费用与资金可用日",
    "func_kzz_hstk201.jsn": "可转债回售条款与触发进度",
    "func_kzz_lltk201.jsn": "可转债逐年票面利率",
    "func_kzz_shtk201.jsn": "可转债赎回条款与触发进度",
    "func_kzz_tkjd201.jsn": "可转债转股、回售、赎回和到期进度",
    "func_kzz_xztk201.jsn": "可转债下修条款与触发进度",
    "kjhz_kjhzsy201_1.jsn": "可交换债概览、评级与付息序列",
    "kzz_kzzsy201_1.jsn": "可转债概览、评级、余额与付息序列",
    "ggxc/$$$SC$$$$$ZQDM$$.jsn": "单证券高管姓名、职务、年薪和履历",
    "cgfxmx1/$$$SC$$$$$ZQDM$$.jsn": "单证券历期机构分类持仓与季度变化",
    "cgfxmx2/$$$SC$$$$$ZQDM$$.jsn": "单证券十大流通股东、持股比例与增减",
    "zttzty/$$$ZQDM$$.jsn": "主题投资逻辑与详情",
    "lhbfx/$$$ZQDM$$.jsn": "龙虎榜事件逐营业部买卖、成功率与预估收益",
    "zcjc/$$$SC$$$$$ZQDM$$.jsn": "单证券近一年股东增减持完整明细",
    "gqzy/$$$SC$$$$$ZQDM$$.jsn": "单证券股权质押完整明细",
    "xtzy/$$$ZQDM$$.jsn": "单信托或券商的股权质押证券明细",
    "dzjy1/$$$ZQDM$$.jsn": "单月大宗交易行业统计",
    "dzjy2/$$$ZQDM$$.jsn": "单月单行业的大宗交易证券明细",
    "dzjy3/$$$SC$$$$$ZQDM$$.jsn": "单证券历史大宗交易成交与营业部明细",
    "dzjy13/$$$SC$$$$$ZQDM$$.jsn": "单证券大宗交易意向申报历史",
    "cggg/$$$SC$$$$$ZQDM$$.jsn": "单证券董监高持股变动完整历史",
    "gdzjc1/$$$ZQDM$$.jsn": "单月股东增减持家数日序列",
    "gghg/$$$SC$$$$$ZQDM$$.jsn": "单只港股股份回购逐笔历史",
    "gqgg/$$$ZQDM$$.jsn": "股权关联分组的证券明细、实控人与走势",
    "rzrq1/$$$SC$$$$$ZQDM$$.jsn": "单证券近三月融资融券完整明细",
    "rzrq2/$$$SC$$$$$ZQDM$$.jsn": "单证券近三月融资与融券余量走势",
    "rzrq3/$$$SC$$$$$ZQDM$$.jsn": "单 ETF 近三月融资融券完整明细",
    "rzrq4/$$$SC$$$$$ZQDM$$.jsn": "单 ETF 近三月融资与融券余量走势",
    "rzrq5/$$$ZQDM$$.jsn": "单行业、概念或风格的长期两融走势",
    "rzrq6/$$$ZQDM$$.jsn": "指定日期的行业两融统计",
    "rzrq7/$$$ZQDM$$.jsn": "指定日期的概念两融统计",
    "rzrq8/$$$ZQDM$$.jsn": "指定日期的风格两融统计",
    "hsgt/$$$ZQDM$$.jsn": "单日单通道十大成交活跃股票",
    "hsgtcg1/$$$SC$$$$$ZQDM$$.jsn": "单证券港股通持股历史明细",
    "hsgtcg2/$$$SC$$$$$ZQDM$$.jsn": "单证券港股通持股比例与净买额走势",
    "hsgtcg1/$$$ZQDM$$.jsn": "季度陆股通证券持股历史明细",
    "hsgtcg2/$$$ZQDM$$.jsn": "季度陆股通证券持股比例与净买额走势",
    "ggthy/$$$SC$$$$$ZQDM$$.jsn": "港股行业资金流及行业证券明细",
    "ggthy1/$$$SC$$$$$ZQDM$$.jsn": "港股行业资金流历史走势",
    "hylhb13801/$$$SC$$$$$ZQDM$$.jsn": "近五日活跃股票的龙虎榜历史异动",
    "hylhb13901/$$$SC$$$$$ZQDM$$.jsn": "近一月活跃股票的龙虎榜历史异动",
    "hylhb14001/$$$SC$$$$$ZQDM$$.jsn": "近半年活跃股票的龙虎榜历史异动",
    "cjrl/$$$ZQDM$$.jsn": "热点会议关联证券成员",
    "qhtj1/$$$SC$$$$$ZQDM$$.jsn": "单期货品种的关联 A 股及多周期涨跌幅",
    "qhtj2/$$$SC$$$$$ZQDM$$.jsn": "单股指期货品种的净持仓历史走势",
    "ipotj102/$$$ZQDM$$.jsn": "单年度 IPO 行业数量、募资与最大项目统计",
    "ipotj103/$$$ZQDM$$.jsn": "单年度单行业 IPO 股票、上市日与募资明细",
    "ipotj104/$$$ZQDM$$.jsn": "单年度 IPO 月度募资额与上市家数走势",
    "bygtj1/$$$ZQDM$$.jsn": "指定日期百元股名单、价格与涨停次数",
    "bygtj3/$$$ZQDM$$.jsn": "百元股家数历史走势",
    "zdtfx1/$$$SC$$$$$ZQDM$$.jsn": "单证券历史涨跌停日期、原因与连板天数",
    "zdtfx2/$$$ZQDM$$.jsn": "指定日期涨停股票、原因与开板次数",
    "zdtfx3/$$$ZQDM$$.jsn": "指定日期跌停股票、原因与开板次数",
    "zjtc1/$$$ZQDM$$.jsn": "单涨价主题的关联股票与入选逻辑",
    "zjtc2/$$$ZQDM$$.jsn": "单涨价主题的历史驱动事件",
    "zjtc3/$$$ZQDM$$.jsn": "单驱动事件的关联股票",
    "zjtc4/$$$ZQDM$$.jsn": "单商品的关联股票、投资逻辑与说明",
    "zjtc5/$$$ZQDM$$.jsn": "单商品的关联行业与 ETF",
    "jjzb1/$$$ZQDM$$.jsn": "单项经济指标的历史数值走势",
    "jjzb2/$$$ZQDM$$.jsn": "单项经济指标的关联行业与股票",
    "ggjx/$$$SC$$$$$ZQDM$$.jsn": "单证券精选公告历史及公告前后表现",
    "ydyl1/$$$ZQDM$$.jsn": "单行业或区域机会组的股票、逻辑与说明",
    "ztxx/$$$ZQDM$$.jsn": "单专题的完整信息时间线",
    "ygzl/$$$ZQDM$$.jsn": "单强势股区间的逐日涨停原因与市场温度",
    "sjqd/$$$ZQDM$$.jsn": "单新闻或部委事件的关联股票",
    "yjyg/$$$ZQDM$$.jsn": "单行业单报告期的公司业绩预告明细",
    "zdjjzczc/$$$SC$$$$$ZQDM$$.jsn": "持有单只股票的主动基金、持仓市值、数量与净值占比",
    "ggpj/$$$SC$$$$$ZQDM$$.jsn": "单只港股的机构评级、目标价与研报理由",
    "hypj/$$$SC$$$$$ZQDM$$.jsn": "单个行业的机构评级、评级变化与研报理由",
    "kzz_hstk/$$$SC$$$$$ZQDM$$.jsn": "单只可转债历次触发回售、价格、数量与金额",
    "kzz_shtk/$$$SC$$$$$ZQDM$$.jsn": "单只可转债历次触发赎回、价格、数量与金额",
    "kzz_xztk/$$$SC$$$$$ZQDM$$.jsn": "单只可转债历次转股价调整日期、价格与原因",
    "hgrztj21701/$$$ZQDM$$.jsn": "全市场单年股份回购月度趋势",
    "hgrztj21702/$$$ZQDM$$.jsn": "A 股单年股份回购月度趋势",
    "hgrztj21703/$$$ZQDM$$.jsn": "港股单年股份回购月度趋势",
    "yybph22401/$$$ZQDM$$.jsn": "营业部近一月大宗交易逐笔明细",
    "yybph22402/$$$ZQDM$$.jsn": "营业部近三月大宗交易逐笔明细",
    "yybph22403/$$$ZQDM$$.jsn": "营业部近半年大宗交易逐笔明细",
    "yybph22404/$$$ZQDM$$.jsn": "营业部近一年大宗交易逐笔明细",
    "lsyd22801/$$$SC$$$$$ZQDM$$.jsn": "单证券最近一周机构龙虎榜异动历史",
    "lsyd22802/$$$SC$$$$$ZQDM$$.jsn": "单证券最近一月机构龙虎榜异动历史",
    "lsyd22803/$$$SC$$$$$ZQDM$$.jsn": "单证券最近三月机构龙虎榜异动历史",
    "lsyd22804/$$$SC$$$$$ZQDM$$.jsn": "单证券最近一年机构龙虎榜异动历史",
    "func_bkld101_1.jsn": "A股板块联动与多周期异动统计",
    "func_bkld102_1.jsn": "行业板块联动与多周期异动统计",
    "func_bkld103_1.jsn": "概念板块联动与多周期异动统计",
    "func_bkld104_1.jsn": "地域板块联动与多周期异动统计",
    "func_scrd101_1.jsn": "市场关注度、专业关注度、共鸣度与舆情热度",
    "func_rdyc101_1.jsn": "热点事件预测、关联股票与预测理由",
    "func_ldph101_1.jsn": "公司亮点数量、类型、详情与安全分",
    "func_sjqd101_1.jsn": "事件清单、关联股票与事件内容",
    "func_bwyq101_1.jsn": "部委要闻事件、关联股票与事件内容",
    "func_bxgc101_1.jsn": "风险观察类型、详情与安全分",
    "func_qzbl101_1.jsn": "潜在爆雷类型、详情与安全分",
    "func_lbtt101_1.jsn": "连板梯队、涨停/炸板与高度统计",
    "func_phcje101_1.jsn": "盘后成交额、开盘/盘后成交与区间涨跌",
    "func_cgfx101_1.jsn": "全部机构持仓家数、股数、占比与季度变动",
    "func_cgfx102_1.jsn": "券商持仓家数、股数、占比与季度变动",
    "func_cgfx103_1.jsn": "保险持仓家数、股数、占比与季度变动",
    "func_cgfx104_1.jsn": "社保持仓家数、股数、占比与季度变动",
    "func_cgfx105_1.jsn": "私募持仓家数、股数、占比与季度变动",
    "func_cgfx106_1.jsn": "公募持仓家数、股数、占比与季度变动",
    "func_cgfx107_1.jsn": "银行持仓家数、股数、占比与季度变动",
    "func_cgfx108_1.jsn": "财务公司持仓家数、股数、占比与季度变动",
    "func_cgfx109_1.jsn": "年金持仓家数、股数、占比与季度变动",
    "func_cgfx110_1.jsn": "一般法人持仓家数、股数、占比与季度变动",
    "func_cgfx111_1.jsn": "QFII 持仓家数、股数、占比与季度变动",
    "func_cgfx112_1.jsn": "信托持仓家数、股数、占比与季度变动",
    "func_cgfx113_1.jsn": "特殊法人持仓家数、股数、占比与季度变动",
    "func_cgfx114_1.jsn": "养老金持仓股数、占比、市值与利润增长",
    "func_cgfx115_1.jsn": "自由流通股本、机构与大股东持仓结构",
    "func_cgfx116_1.jsn": "前期抱团股超跌与机构持仓变化",
    "func_cgfx117_1.jsn": "北向资金持仓家数、股数、占比与季度变动",
    "func_cgfxhy101_1.jsn": "一季度行业机构持仓与环比变化",
    "func_cgfxhy103_1.jsn": "半年报行业机构持仓与环比变化",
    "func_cgfxhy104_1.jsn": "三季度行业机构持仓与环比变化",
    "func_cgfxhy105_1.jsn": "年报行业机构持仓与环比变化",
    "func_tzcg104_1.jsn": "汇金、证金持仓比例、股东位次与报告期",
    "func_jgcg108_1.jsn": "知名私募管理人持股市值、占流通比与报告期",
    "func_tbgz108_1.jsn": "基金独门持仓、基金公司、持股数量与占比",
    "func_gdrs101_1.jsn": "沪市主板股东人数及变化",
    "func_gdrs102_1.jsn": "深市主板股东人数及变化",
    "func_gdrs104_1.jsn": "创业板股东人数及变化",
    "func_gdrs106_1.jsn": "科创板股东人数及变化",
    "func_gdrs107_1.jsn": "北证 A 股股东人数及变化",
    "func_ggyjpl101_1.jsn": "公司业绩评论",
    "func_ggyjyg101_1.jsn": "公司业绩预告",
    "func_cggg101_1.jsn": "最新董监高持股变动、成交价、数量与关系",
    "func_gdzjc102_1.jsn": "股东增减持按月金额、家数与净额统计",
    "func_gghg101_1.jsn": "港股最新股份回购、价格、数量与金额",
    "func_gqgg101_1.jsn": "股权关联行业分组及证券集合",
    "func_gqgg102_1.jsn": "地方国资改革分组及证券集合",
    "func_gqgg103_1.jsn": "央企整合等整合关系分组及证券集合",
    "func_gqgg104_1.jsn": "公司系关系分组及证券集合",
    "func_rzrq101_1.jsn": "融资融券按市场日序列统计",
    "func_rzrq102_1.jsn": "两融差额最大的证券排行",
    "func_rzrq103_1.jsn": "融资余额占流通市值比例最高排行",
    "func_rzrq104_1.jsn": "融券余量占流通股比例最高排行",
    "func_rzrq107_1.jsn": "融资余额连续减少证券",
    "func_rzrq108_1.jsn": "融券余量大幅增加证券",
    "func_rzrq109_1.jsn": "融券余量大幅减少证券",
    "func_rzrq110_1.jsn": "融资余额连续增加证券",
    "func_rzrq111_1.jsn": "融资余额大幅增加证券",
    "func_rzrq112_1.jsn": "融资余额大幅减少证券",
    "func_rzrq113_1.jsn": "融券余量连续增加证券",
    "func_rzrq114_1.jsn": "融券余量连续减少证券",
    "func_rzrq120_1.jsn": "融券余额高及余额占比排行",
    "func_rzrq201_1.jsn": "ETF 融资融券余额、余量与差额排行",
    "func_hsgt101_1.jsn": "沪股通日度资金流与指数表现",
    "func_hsgt102_1.jsn": "港股通沪通道日度资金流",
    "func_hsgt103_1.jsn": "深股通日度资金流与指数表现",
    "func_hsgt104_1.jsn": "港股通深通道日度资金流",
    "func_hsgt113_1.jsn": "陆股通日度合计资金流",
    "func_hsgt114_1.jsn": "港股通日度合计资金流",
    "func_gghq_hsgt_lgt_1.jsn": "陆股通长期周度资金流",
    "func_gghq_hsgt_ggt_1.jsn": "港股通长期周度资金流",
    "func_hsgt201_1.jsn": "港股通证券持股、占比与净买入",
    "func_hsgt202_1.jsn": "港股通调出证券及历史持股",
    "func_hsgt204_1.jsn": "陆股通调出证券及历史持股",
    "func_hsgt205_1.jsn": "陆股通单日大幅增仓证券",
    "func_hsgt206_1.jsn": "陆股通单日大幅减仓证券",
    "func_hsgt207_1.jsn": "陆股通连续增仓证券",
    "func_hsgt208_1.jsn": "陆股通连续减仓证券",
    "func_hsgt211_1.jsn": "陆股通频繁增仓证券",
    "func_hsgt212_1.jsn": "陆股通季度持仓数量、比例与市值",
    "func_hsgt301_1.jsn": "港股通行业资金流汇总",
    "func_hylhb101_1.jsn": "近五日活跃龙虎榜股票",
    "func_hylhb102_1.jsn": "近一月活跃龙虎榜股票",
    "func_hylhb104_1.jsn": "近半年活跃龙虎榜股票",
    "func_cjrl101_1.jsn": "全球宏观经济数据日历",
    "func_cjrl105_1.jsn": "热点会议、重要度、地区与关联行业",
    "func_ggrl101_1.jsn": "新股申购、上市与首日表现日历",
    "func_dzjy104_1.jsn": "大宗交易明细",
    "func_rdhs101_1.jsn": "热点事件时间线",
    "func_rdhs102_1.jsn": "热点事件时间线明细",
    "func_qhtj101_1.jsn": "大宗商品期货行情、持仓与多周期涨跌统计",
    "func_qhtj103_1.jsn": "国内期货月度成交量与成交额统计",
    "func_qhtj104_1.jsn": "股指期货主力合约行情",
    "func_ipotj101_1.jsn": "历年 IPO 家数、募资额、最大项目与指数表现",
    "zq_ssfxr201.jsn": "上市公司债券发行人及对应股票",
    "zq_wssfxr201.jsn": "未上市债券发行人名录",
    "func_bygtj102_1.jsn": "百元股家数、涨跌分布与成交额日序列",
    "func_zdtfx101_1.jsn": "最新涨停股票、原因、时间与开板次数",
    "func_zdtfx102_1.jsn": "最新跌停股票、原因、时间与开板次数",
    "func_zdtfx103_1.jsn": "含北交所的最新涨停股票与触发原因",
    "func_zdtfx106_1.jsn": "证券年度涨停跌停次数统计",
    "func_zdtfx107_1.jsn": "全市场涨跌停家数与连板高度日序列",
    "func_zjtc101_1.jsn": "涨价主题、逻辑、最新事件与关联股票集合",
    "func_zjtc103_1.jsn": "商品现货行情与多周期价格变化",
    "func_dpyd101_1.jsn": "历史大盘异动、市场宽度、成交与事后表现",
    "func_etfsg101_1.jsn": "单股票 ETF 持股规模、申赎流与交易资金流",
    "func_etfsg102_1.jsn": "行业板块 ETF 规模、申赎流与交易资金流",
    "func_rdhs101_1.jsn": "热点板块龙头区间、连板表现与操作方向",
    "func_rdhs102_1.jsn": "当前热点股票区间与相对指数表现",
    "func_jjzb101_1.jsn": "经济与商品指标最新值、同比环比和报告期",
    "func_zxjx101_1.jsn": "上市公司精选公告、多空分类与公告后表现",
    "func_zxjx103_1.jsn": "上市公司交易风险公告与前后价格表现",
    "func_ydyl101_1.jsn": "行业机会组及其证券成员",
    "func_ydyl102_1.jsn": "区域机会组及其证券成员",
    "func_ztxx101_1.jsn": "公司与宏观专题目录、描述及更新时间",
    "func_xwlb101_1.jsn": "新闻联播要闻历史全文",
    "func_jysjk101_1.jsn": "当前交易监管期股票、期限与异动公告",
    "func_jysjk102_1.jsn": "已结束交易监管股票、期限与区间价格",
    "func_ygzl101_1.jsn": "强势股连板生命周期、区间涨幅与市场对照",
    "func_lhbfx101_1.jsn": "当日龙虎榜还原、买卖额、净买入与席位数",
    "func_lhbfx103_1.jsn": "龙虎榜机构参与、机构净买入与席位数",
    "func_lhbfx104_1.jsn": "龙虎榜市场风口股票",
    "func_lhbfx105_1.jsn": "龙虎榜合力封板股票",
    "func_lhbfx106_1.jsn": "龙虎榜一家独大股票",
    "func_lhbfx107_1.jsn": "龙虎榜量化席位买卖与净买入",
    "func_lhbfx108_1.jsn": "龙虎榜同城营业部协同交易",
    "func_lhbfx110_1.jsn": "龙虎榜游资席位买卖与净买入",
    "func_qszj101_1.jsn": "5 日资金强势、总/主力净流入与 DDX",
    "func_qszj102_1.jsn": "10 日资金强势、总/主力净流入与 DDX",
    "func_qszj103_1.jsn": "20 日资金强势、总/主力净流入与 DDX",
    "func_qszj104_1.jsn": "30 日资金强势、总/主力净流入与 DDX",
    "func_qszj105_1.jsn": "近三个月资金强势、总/主力净流入与 DDX",
    "func_zcjc101_1.jsn": "最新股东增持、价格、数量与公告日期",
    "func_zcjc102_1.jsn": "增持比例最高及区间价格与收益",
    "func_zcjc103_1.jsn": "增持市值最高及占流通市值比例",
    "func_zcjc104_1.jsn": "增持次数最多及净增持数量",
    "func_zcjc105_1.jsn": "拟增持计划、期限、规模与增持人",
    "func_zcjc106_1.jsn": "承诺不减持期限、承诺人与详情",
    "func_zcjc107_1.jsn": "最新股东减持、价格、数量与公告日期",
    "func_zcjc108_1.jsn": "减持比例最高及区间价格与收益",
    "func_zcjc109_1.jsn": "减持市值最高及占流通市值比例",
    "func_zcjc110_1.jsn": "减持次数最多及净减持数量",
    "func_zcjc111_1.jsn": "拟减持计划、期限、规模与减持人",
    "func_gqzy101_1.jsn": "最新股权质押关系、数量、比例与日期",
    "func_gqzy102_1.jsn": "股权质押预警价格与预警距离",
    "func_gqzy103_1.jsn": "股权质押平仓价格与平仓距离",
    "func_gqzy105_1.jsn": "股权质押按月份统计",
    "func_gqzy106_1.jsn": "股权质押按信托机构统计",
    "func_gqzy108_1.jsn": "股权质押按券商统计",
    "func_gqzy109_1.jsn": "最新股权解押关系、数量与日期",
    "func_gsrl206_1.jsn": "分红转增实施与预案日历",
    "func_hgrztj101_1.jsn": "全市场股份回购汇总",
    "func_hgrztj102_1.jsn": "A 股股份回购汇总",
    "func_hgrztj103_1.jsn": "港股股份回购汇总",
    "func_hgtj101_1.jsn": "股份回购月度拟回购与实际完成趋势",
    "func_jqgz103_1.jsn": "近期大比例限售解禁",
    "func_qxfa104_1.jsn": "股份回购方案、进度、价格、金额与用途",
    "func_dzjy101_1.jsn": "大宗交易按月份成交额、折溢价与次数",
    "func_dzjy104_1.jsn": "个股大宗交易明细与买卖营业部",
    "func_dzjy107_1.jsn": "近一月大宗交易营业部画像与成功率",
    "func_dzjy108_1.jsn": "近三月大宗交易营业部画像与成功率",
    "func_dzjy109_1.jsn": "近半年大宗交易营业部画像与成功率",
    "func_dzjy1010_1.jsn": "近一年大宗交易营业部画像与成功率",
    "func_dzjy1012_1.jsn": "大宗交易意向申报、价格、数量与方向",
    "func_yjygtj101_1.jsn": "行业和报告期业绩预告类型分布与公司数量",
    "func_zdjjzczc101_1.jsn": "主动基金增持股票、持仓市值、数量与基金数",
    "func_jgzc101_1.jsn": "最近一周机构席位买卖与净买入排行",
    "func_jgzc102_1.jsn": "最近一月机构席位买卖与净买入排行",
    "func_jgzc103_1.jsn": "最近三月机构席位买卖与净买入排行",
    "func_jgzc104_1.jsn": "最近一年机构席位买卖与净买入排行",
    "func_ggpj101_1.jsn": "港股最新机构评级、目标价与机构数",
    "func_hypj101_1.jsn": "行业最新机构评级、多空观点与机构数",
}


class CatalogError(ValueError):
    """Raised when a downloaded JSN resource has an invalid table shape."""


def description_for(template: str) -> str:
    return RESOURCE_DESCRIPTIONS.get(
        template,
        RESOURCE_DESCRIPTIONS.get(PurePosixPath(template).name, ""),
    )


def template_pattern(template: str) -> re.Pattern[str]:
    normalized = downloader.normalize_resource_path(template)
    parts: list[str] = []
    cursor = 0
    for match in downloader.TEMPLATE_TOKEN_RE.finditer(normalized):
        parts.append(re.escape(normalized[cursor : match.start()]))
        parts.append(r"[^/]+")
        cursor = match.end()
    parts.append(re.escape(normalized[cursor:]))
    return re.compile("^" + "".join(parts) + "$", re.I)


def template_parameters(
    template: str,
    relative_path: str,
) -> dict[str, str]:
    normalized = downloader.normalize_resource_path(template)
    parts: list[str] = []
    names: list[str] = []
    cursor = 0
    for index, match in enumerate(
        downloader.TEMPLATE_TOKEN_RE.finditer(normalized)
    ):
        token = match.group(0)
        if token == "$$$SC$$" and normalized.startswith(("ggthy/", "ggthy1/")):
            name = "group_market"
        elif token == "$$$SC$$":
            name = "market"
        elif token == "$$$ZQDM$$" and normalized.startswith("lhbfx/"):
            name = "event_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("zttzty/"):
            name = "theme_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("xtzy/"):
            name = "institution_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("dzjy1/"):
            name = "month"
        elif token == "$$$ZQDM$$" and normalized.startswith("dzjy2/"):
            name = "industry_period_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("gdzjc1/"):
            name = "month"
        elif token == "$$$ZQDM$$" and normalized.startswith("gqgg/"):
            name = "group_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("rzrq5/"):
            name = "block_id"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("rzrq6/", "rzrq7/", "rzrq8/")
        ):
            name = "date"
        elif token == "$$$ZQDM$$" and normalized.startswith("hsgt/"):
            name = "activity_key"
        elif (
            token == "$$$ZQDM$$"
            and normalized.startswith(("hsgtcg1/", "hsgtcg2/"))
            and "$$$SC$$" not in normalized
        ):
            name = "holding_key"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("ggthy/", "ggthy1/")
        ):
            name = "group_code"
        elif token == "$$$ZQDM$$" and normalized.startswith("cjrl/"):
            name = "calendar_event_id"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("ipotj102/", "ipotj104/")
        ):
            name = "year"
        elif token == "$$$ZQDM$$" and normalized.startswith("ipotj103/"):
            name = "ipo_industry_key"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("bygtj1/", "bygtj3/")
        ):
            name = "premium_group_key"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("zdtfx2/", "zdtfx3/")
        ):
            name = "date"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("zjtc1/", "zjtc2/")
        ):
            name = "price_theme_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("zjtc3/"):
            name = "price_event_id"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("zjtc4/", "zjtc5/")
        ):
            name = "commodity_id"
        elif token == "$$$ZQDM$$" and normalized.startswith(
            ("jjzb1/", "jjzb2/")
        ):
            name = "economic_indicator_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("ydyl1/"):
            name = "opportunity_group_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("ztxx/"):
            name = "topic_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("ygzl/"):
            name = "strength_interval_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("sjqd/"):
            name = "news_event_id"
        elif token == "$$$ZQDM$$" and normalized.startswith("yjyg/"):
            name = "forecast_group_key"
        elif token == "$$$ZQDM$$" and normalized.startswith("hgrztj"):
            name = "year"
        elif token == "$$$ZQDM$$" and normalized.startswith("yybph"):
            name = "branch_id"
        elif token == "$$$ZQDM$$":
            name = "code"
        else:
            name = f"parameter_{index + 1}"
        # Domestic markets use one digit, while the same JSN mechanism also
        # carries two-digit Hong Kong/overseas market IDs.  Keep the known
        # two-digit alternatives first because SC and ZQDM are concatenated.
        expression = (
            r"(?:28|29|30|31|44|48|66|70|\d)"
            if token == "$$$SC$$"
            else r"[^/]+"
        )
        parts.append(re.escape(normalized[cursor : match.start()]))
        parts.append(f"(?P<{name}>{expression})")
        names.append(name)
        cursor = match.end()
    if not names:
        return {}
    parts.append(re.escape(normalized[cursor:]))
    match = re.fullmatch("".join(parts), relative_path, re.I)
    return match.groupdict() if match else {}


def identify_resource(
    relative_path: str,
    resources: Sequence[downloader.JsnResource],
) -> downloader.JsnResource | None:
    normalized = PurePosixPath(relative_path).as_posix()
    for resource in resources:
        if not template_pattern(resource.resource).fullmatch(normalized):
            continue
        # Multiple templates may share one directory.  For example current
        # Stock Connect details use either ``<SC><ZQDM>`` or ``jd<code>``.
        # Reject a broad structural match when its typed parameters cannot
        # actually be parsed, then try the next template.
        if resource.placeholders and not template_parameters(
            resource.resource, normalized
        ):
            continue
        return resource
    return None


def column_metadata(
    root: Path,
    source_files: Sequence[str],
    template: str,
) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    directory = root / "T0002" / "cloud_cfg"
    for filename in source_files:
        text = cloud.COMMENT_RE.sub(
            "",
            cloud.read_text_guess(directory / filename),
        )
        for match in cloud.ELEMENT_RE.finditer(text):
            if match.group("tag").casefold() != "gridcol":
                continue
            attributes = cloud.parse_attributes(match.group("attrs"))
            name = attributes.get("name", "")
            if not name or name in result:
                continue
            result[name] = {
                key: attributes.get(key, "")
                for key in (
                    "caption",
                    "datatype",
                    "visible",
                    "syscol",
                    "digital",
                )
            }

    resource_stem = PurePosixPath(
        downloader.normalize_resource_path(template)
    ).stem
    if resource_stem.endswith("_1"):
        resource_stem = resource_stem[:-2]
    cfg_names = {
        Path(filename).with_suffix(".cfg").name
        for filename in source_files
    }
    cfg_names.add(f"{resource_stem}.cfg")
    cfg_names.add(f"func_{resource_stem}.cfg")
    if resource_stem.startswith("dfkzz"):
        cfg_names.add(f"func_kzz_{resource_stem}.cfg")
    for filename in sorted(cfg_names, key=str.casefold):
        path = directory / filename
        if not path.is_file():
            continue
        text = cloud.COMMENT_RE.sub("", cloud.read_text_guess(path))
        for match in cloud.ELEMENT_RE.finditer(text):
            if match.group("tag").casefold() != "item":
                continue
            attributes = cloud.parse_attributes(match.group("attrs"))
            name = attributes.get("code", "")
            caption = attributes.get("name", "")
            if not name:
                continue
            current = result.setdefault(
                name,
                {
                    "caption": "",
                    "datatype": "",
                    "visible": "",
                    "syscol": "",
                    "digital": "",
                },
            )
            if caption:
                current["caption"] = caption
            if attributes.get("datatype"):
                current["datatype"] = attributes["datatype"]
            if "hide" in attributes:
                current["visible"] = (
                    "false"
                    if attributes["hide"].casefold() in {"1", "true"}
                    else "true"
                )
            if attributes.get("syscol"):
                current["syscol"] = attributes["syscol"]
    # The live redemption-history table uses ``WSHSL`` while both the CFG
    # and the commented XML pane spell the same field ``WHSSL``.  Preserve
    # the wire header and copy only its documented presentation metadata.
    if template.startswith("kzz_shtk/") and "WHSSL" in result:
        result.setdefault("WSHSL", dict(result["WHSSL"]))
    return result


def load_tables(path: Path) -> list[tuple[list[str], list[list[object]]]]:
    value = json.loads(path.read_bytes().decode("gb18030"))
    if not isinstance(value, list):
        raise CatalogError(f"{path.name}: 根节点不是数组")
    tables: list[tuple[list[str], list[list[object]]]] = []
    for group_index, group in enumerate(value):
        if not isinstance(group, dict):
            raise CatalogError(f"{path.name}: 第 {group_index} 个结果集不是对象")
        header = group.get("colheader")
        data = group.get("data")
        if not isinstance(header, list) or not isinstance(data, list):
            raise CatalogError(
                f"{path.name}: 第 {group_index} 个结果集缺少 colheader/data"
            )
        names = [str(item) for item in header]
        rows: list[list[object]] = []
        for row_index, row in enumerate(data):
            if not isinstance(row, list) or len(row) != len(names):
                raise CatalogError(
                    f"{path.name}: 第 {group_index}/{row_index} 行列数不符"
                )
            rows.append(row)
        tables.append((names, rows))
    return tables


def security_key_fields(headers: Sequence[str]) -> tuple[str, str] | None:
    if "$SC" in headers and "$ZQDM" in headers:
        return "$SC", "$ZQDM"
    if "$SC1" in headers and "$ZQDM1" in headers:
        return "$SC1", "$ZQDM1"
    if "sc" in headers and "$ZQDM" in headers:
        return "sc", "$ZQDM"
    return None


def member_security_keys(value: object) -> tuple[tuple[str, str], ...]:
    result: list[tuple[str, str]] = []
    for token in str(value).split(","):
        market, separator, code = token.strip().partition("|")
        if separator and market.strip() and code.strip():
            result.append((market.strip(), code.strip()))
    return tuple(result)


def analyze_file(
    path: Path,
    input_dir: Path,
    root: Path,
    resources: Sequence[downloader.JsnResource],
) -> dict[str, object]:
    relative_path = path.relative_to(input_dir).as_posix()
    resource = identify_resource(relative_path, resources)
    template = resource.resource if resource else relative_path
    source_files = resource.source_files if resource else ()
    metadata = column_metadata(root, source_files, template)
    tables = load_tables(path)
    rows = sum(len(table_rows) for _, table_rows in tables)

    all_headers: list[str] = []
    for headers, _ in tables:
        all_headers.extend(headers)
    headers = list(dict.fromkeys(all_headers))
    key_fields = security_key_fields(headers)
    security_keys: list[tuple[str, str]] = []
    if key_fields:
        market_field, code_field = key_fields
        for table_headers, table_rows in tables:
            if market_field not in table_headers or code_field not in table_headers:
                continue
            market_index = table_headers.index(market_field)
            code_index = table_headers.index(code_field)
            security_keys.extend(
                (
                    str(row[market_index]),
                    str(row[code_index]),
                )
                for row in table_rows
                if str(row[code_index])
            )
    unique_keys = set(security_keys)
    markets = Counter(market for market, _ in unique_keys)
    member_keys: list[tuple[str, str]] = []
    for table_headers, table_rows in tables:
        if "$S_ZQDM" not in table_headers:
            continue
        member_index = table_headers.index("$S_ZQDM")
        for row in table_rows:
            member_keys.extend(member_security_keys(row[member_index]))
    unique_member_keys = set(member_keys)
    member_markets = Counter(market for market, _ in unique_member_keys)
    raw = path.read_bytes()
    return {
        "resource": relative_path,
        "template": template,
        "template_parameters": template_parameters(template, relative_path),
        "description": description_for(template),
        "source_files": list(source_files),
        "size": len(raw),
        "md5": hashlib.md5(raw).hexdigest(),
        "groups": len(tables),
        "rows": rows,
        "key_fields": list(key_fields) if key_fields else [],
        "unique_securities": len(unique_keys),
        "duplicate_security_rows": len(security_keys) - len(unique_keys),
        "markets": dict(sorted(markets.items())),
        "member_fields": ["$S_ZQDM"] if "$S_ZQDM" in headers else [],
        "member_security_links": len(member_keys),
        "unique_member_securities": len(unique_member_keys),
        "member_markets": dict(sorted(member_markets.items())),
        "columns": [
            {
                "name": name,
                **metadata.get(
                    name,
                    {
                        "caption": "",
                        "datatype": "",
                        "visible": "",
                        "syscol": "",
                        "digital": "",
                    },
                ),
            }
            for name in headers
        ],
        "unmapped_fields": [
            name for name in headers if not metadata.get(name, {}).get("caption")
        ],
    }


def build_catalog(root: Path, input_dir: Path) -> dict[str, object]:
    if not input_dir.is_dir():
        raise CatalogError(f"JSN 下载目录不存在：{input_dir}")
    resources = downloader.merge_resource_inventories(
        downloader.inventory_resources(root),
        downloader.inventory_cfg_resources(root),
    )
    paths = sorted(
        input_dir.rglob("*.jsn"),
        key=lambda item: item.relative_to(input_dir).as_posix().casefold(),
    )
    records = [
        analyze_file(path, input_dir, root, resources)
        for path in paths
    ]
    return {
        "schema": "tdx-jsn-catalog-v1",
        "counts": {
            "resources": len(records),
            "bytes": sum(int(item["size"]) for item in records),
            "rows": sum(int(item["rows"]) for item in records),
            "unique_templates": len(
                {str(item["template"]) for item in records}
            ),
        },
        "resources": records,
    }


def render_markdown(catalog: dict[str, object]) -> str:
    counts = catalog["counts"]
    lines = [
        "# 通达信 JSN 资源目录",
        "",
        f"- 文件：{counts['resources']}",
        f"- 总字节：{counts['bytes']}",
        f"- 总行数：{counts['rows']}",
        "",
        "| 资源 | 内容 | 字节 | 行数 | 唯一证券 | 重复证券行 |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for item in catalog["resources"]:
        lines.append(
            f"| `{item['resource']}` | {item['description'] or '—'} | "
            f"{item['size']} | {item['rows']} | "
            f"{item['unique_securities']} | "
            f"{item['duplicate_security_rows']} |"
        )
    lines.append("")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "扫描已下载的 reqformat=11 JSN，结合 cloud_cfg 导出中文字段、"
            "证券覆盖、重复记录、大小和 MD5。该工具不联网。"
        )
    )
    parser.add_argument("--root", type=Path, help="通达信安装目录；默认自动寻找")
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn",
        help="已下载 JSN 目录",
    )
    parser.add_argument(
        "--format",
        choices=("json", "markdown"),
        default="json",
        help="目录格式",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=updater.PROJECT_ROOT / "output" / "tdx-jsn-catalog.json",
        help="输出文件",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        root = updater.find_tdx_root(args.root)
        input_dir = args.input_dir.expanduser().resolve()
        catalog = build_catalog(root, input_dir)
        if args.format == "markdown":
            text = render_markdown(catalog)
        else:
            text = json.dumps(catalog, ensure_ascii=False, indent=2)
        output = args.output.expanduser().resolve()
        updater.atomic_write_text(
            output,
            text + ("" if text.endswith("\n") else "\n"),
            "utf-8",
        )
        counts = catalog["counts"]
        print(
            f"已编目 {counts['resources']} 个资源、{counts['rows']} 行、"
            f"{counts['bytes']} 字节：{output}"
        )
    except (
        OSError,
        UnicodeError,
        json.JSONDecodeError,
        re.error,
        updater.UpdateError,
        downloader.JsnDownloadError,
        CatalogError,
    ) as error:
        print(f"编目失败：{error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
