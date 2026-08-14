cmake_minimum_required(VERSION 3.20)
project(tdx_tool VERSION 0.2.0 LANGUAGES CXX)

option(TDX_BUILD_TCALC_EXTRACTOR
       "Build the optional standalone TCalc.dll forensic extractor" OFF)

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /permissive-)
endif()

set(TDX_RECON_SOURCES
    src/recon/recon_inventory.cpp
    src/recon/recon_install_command.cpp
    src/recon/runtime_topology.cpp
    src/recon/runtime_topology_config.cpp
    src/recon/runtime_topology_watch.cpp
    src/recon/runtime_topology_command.cpp
    src/recon/recon_contract_response.cpp
    src/recon/recon_contract_audit.cpp
    src/recon/recon_contract_command.cpp
    src/recon/doctor_command.cpp
    src/recon/recon_contract_catalog.cpp
    src/recon/recon_contract_formula.cpp
    src/recon/recon_contract_formula_foundation_catalog.cpp
    src/recon/recon_contract_formula_foundation_icons.cpp
    src/recon/recon_contract_formula_foundation_coverage.cpp
    src/recon/recon_contract_formula_foundation_capabilities.cpp
    src/recon/recon_contract_formula_foundation_service.cpp
    src/recon/recon_contract_formula_foundation_openapi.cpp
    src/recon/recon_contract_formula_calculation.cpp
    src/recon/recon_contract_formula_render.cpp
    src/recon/recon_contract_formula_render_price.cpp
    src/recon/recon_contract_formula_render_sequence.cpp
    src/recon/recon_contract_formula_render_series.cpp
    src/recon/recon_contract_formula_render_presentation.cpp
    src/recon/recon_contract_formula_render_primitives.cpp
    src/recon/recon_contract_formula_render_background.cpp
    src/recon/recon_contract_formula_runtime.cpp
    src/recon/recon_contract_formula_runtime_math.cpp
    src/recon/recon_contract_formula_runtime_future.cpp
    src/recon/recon_contract_formula_runtime_security.cpp
    src/recon/recon_contract_formula_runtime_data.cpp
    src/recon/recon_contract_formula_runtime_host.cpp
    src/recon/recon_contract_formula_runtime_context.cpp
    src/recon/recon_contract_formula_workflow.cpp
    src/recon/recon_contract_market.cpp
    src/recon/recon_contract_market_data.cpp
    src/recon/recon_contract_market_data_quote.cpp
    src/recon/recon_contract_market_data_institution.cpp
    src/recon/recon_contract_market_data_flow.cpp
    src/recon/recon_contract_market_data_session.cpp
    src/recon/recon_contract_market_data_bonds_catalog.cpp
    src/recon/recon_contract_market_data_bonds_reference.cpp
    src/recon/recon_contract_market_data_bonds_rated.cpp
    src/recon/recon_contract_market_data_bonds_pending.cpp
    src/recon/recon_contract_market_data_bonds_subscriptions.cpp
    src/recon/recon_contract_market_data_bonds_exchangeable.cpp
    src/recon/recon_contract_market_data_bonds_pricing.cpp
    src/recon/recon_contract_market_research.cpp
    src/recon/recon_contract_market_research_catalog_dispatch.cpp
    src/recon/recon_contract_market_research_catalog_forecasts.cpp
    src/recon/recon_contract_market_research_catalog_indicators.cpp
    src/recon/recon_contract_market_research_catalog_strategic.cpp
    src/recon/recon_contract_market_research_catalog_themes.cpp
    src/recon/recon_contract_market_research_catalog_thematic.cpp
    src/recon/recon_contract_market_research_catalog_thematic_hype.cpp
    src/recon/recon_contract_market_research_signals.cpp
    src/recon/recon_contract_market_research_events.cpp
    src/recon/recon_contract_market_research_institutions.cpp
    src/recon/recon_contract_market_research_intelligence.cpp
    src/recon/recon_contract_market_corporate.cpp
    src/recon/recon_contract_market_corporate_catalog.cpp
    src/recon/recon_contract_market_special_situations.cpp
    src/recon/recon_contract_market_corporate_transitions.cpp
    src/recon/recon_contract_market_exchange_funds.cpp
    src/recon/recon_contract_market_etf_share_ranking.cpp
    src/recon/recon_contract_market_curated_data.cpp
    src/recon/recon_contract_market_special_attention.cpp
    src/recon/recon_contract_market_fund_statistics.cpp
    src/recon/recon_contract_market_corporate_company.cpp
    src/recon/recon_contract_market_corporate_insights.cpp
    src/recon/recon_contract_market_corporate_integration_catalog.cpp
    src/recon/recon_contract_market_corporate_integration_calendar.cpp
    src/recon/recon_contract_market_corporate_integration_platform.cpp
    src/recon/recon_contract_market_corporate_integration_rankings.cpp
    src/recon/recon_contract_market_corporate_integration_securities.cpp
    src/recon/recon_contract_market_corporate_integration_unlocks.cpp
    src/recon/recon_contract_market_corporate_ipo.cpp
    src/recon/recon_contract_market_corporate_offerings.cpp
    src/recon/recon_contract_requests.cpp
)

set(TDX_SERVER_SOURCES
    src/server/server.cpp
    src/server/server_request.cpp
    src/server/server_queries.cpp
    src/server/server_response.cpp
    src/server/server_route.cpp
    src/server/server_catalog.cpp
    src/server/server_local_resource_paths.cpp
    src/server/server_formula_support.cpp
    src/server/server_formula_query.cpp
    src/server/server_formula_context_import.cpp
    src/server/server_formula_cloud.cpp
    src/server/server_formula_execution.cpp
    src/server/server_formula_inline.cpp
    src/server/server_formula_backtest.cpp
    src/server/server_formula_scan.cpp
    src/server/server_pool.cpp
    src/server/server_level2.cpp
    src/server/server_level2_project.cpp
    src/server/server_market.cpp
    src/server/server_market_analytics.cpp
    src/server/server_market_catalog_funds.cpp
    src/server/server_market_catalog_discovery.cpp
    src/server/server_market_catalog_corporate.cpp
    src/server/server_market_catalog_analytics.cpp
    src/server/server_market_events.cpp
    src/server/server_market_institution.cpp
    src/server/server_market_realtime.cpp
    src/server/server_market_research.cpp
    src/server/server_state.cpp
)

set(TDX_FORMULA_SOURCES
    src/formula/formula_bundle.cpp
    src/formula/formula_bundle_command.cpp
    src/formula/formulas_catalog.cpp
    src/formula/formulas_model.cpp
    src/formula/formulas_user.cpp
    src/formula/formulas_user_command.cpp
    src/formula/formula_analysis_semantics.cpp
    src/formula/formula_analysis_source.cpp
    src/formula/formula_analysis_library.cpp
    src/formula/formula_analysis_reference.cpp
    src/formula/formula_analysis_context.cpp
    src/formula/formula_context_capture.cpp
    src/formula/formula_context_capture_command.cpp
    src/formula/formula_calc_support.cpp
    src/formula/formula_calc_trend.cpp
    src/formula/formula_calc_volume.cpp
    src/formula/formula_calc_advanced.cpp
    src/formula/formula_calc_registry.cpp
    src/formula/formula_calc_command.cpp
    src/formula/formula_commands.cpp
    src/formula/formula_environment.cpp
    src/formula/formula_environment_calendar.cpp
    src/formula/formula_environment_context.cpp
    src/formula/formula_environment_fields.cpp
    src/formula/formula_engine.cpp
    src/formula/formula_future_replay.cpp
    src/formula/formula_engine_support.cpp
    src/formula/formula_function_support.cpp
    src/formula/formula_functions_calendar.cpp
    src/formula/formula_functions_chip.cpp
    src/formula/formula_functions_context.cpp
    src/formula/formula_functions_presentation.cpp
    src/formula/formula_functions_series.cpp
    src/formula/formula_tdx_indicators_support.cpp
    src/formula/formula_tdx_indicators_chip.cpp
    src/formula/formula_tdx_indicators_signal.cpp
    src/formula/formula_tdx_indicators_trend.cpp
    src/formula/formula_tdx_indicators_band_volume.cpp
    src/formula/formula_tdx_indicators_registry.cpp
    src/formula/formula_language.cpp
    src/formula/formula_functions.cpp
    src/formula/formula_functions_support.cpp
    src/formula/formula_functions_transforms.cpp
    src/formula/formula_functions_windows.cpp
    src/formula/formula_functions_boolean_windows.cpp
    src/formula/formula_functions_window_positions.cpp
    src/formula/formula_functions_host.cpp
    src/formula/formula_functions_scalar.cpp
    src/formula/formula_functions_reference.cpp
    src/formula/formula_functions_future.cpp
    src/formula/formula_functions_core.cpp
    src/formula/formula_registry.cpp
    src/formula/formula_render.cpp
    src/formula/formula_render_common.cpp
    src/formula/formula_render_series.cpp
    src/formula/formula_render_metadata.cpp
    src/formula/formula_render_events.cpp
    src/formula/formula_render_events_segments.cpp
    src/formula/formula_render_events_surfaces.cpp
    src/formula/formula_render_events_sequence.cpp
    src/formula/formula_render_events_regions.cpp
    src/formula/formula_render_events_general.cpp
    src/formula/formula_render_events_annotations.cpp
    src/formula/formula_render_profile.cpp
    src/formula/formula_render_support.cpp
    src/formula/formula_operator_semantics.cpp
    src/formula/formula_runtime.cpp
    src/formula/formula_trade_event_filter.cpp
    src/formula/formula_trade_event_ir.cpp
    src/formula/formula_catalog_internal.cpp
    src/formula/formula_nested.cpp
    src/formula/formula_reference.cpp
    src/formula/formula_context.cpp
    src/formula/formula_context_plan.cpp
    src/formula/formula_context_host.cpp
    src/formula/formula_context_local.cpp
    src/formula/formula_context_finance_extensions.cpp
    src/formula/formula_context_aggregate_support.cpp
    src/formula/formula_context_horcalc.cpp
    src/formula/formula_context_indicator_aggregate.cpp
    src/formula/formula_context_blocks.cpp
    src/formula/formula_context_current_finance.cpp
    src/formula/formula_context_hk_finance.cpp
    src/formula/formula_context_dynamic.cpp
    src/formula/formula_context_finance.cpp
    src/formula/formula_context_market_summary.cpp
    src/formula/formula_context_professional.cpp
    src/formula/formula_context_relations.cpp
    src/formula/formula_context_relation_support.cpp
    src/formula/formula_context_relation_series.cpp
    src/formula/formula_context_relation_identity.cpp
    src/formula/formula_context_relation_industry.cpp
    src/formula/formula_context_support.cpp
    src/formula/formula_scan_support.cpp
    src/formula/formula_scan_universe.cpp
    src/formula/formula_scan_engine.cpp
    src/formula/formula_scan_diff.cpp
    src/formula/formula_scan_watch_state.cpp
    src/formula/formula_scan_command_support.cpp
    src/formula/formula_scan_execute.cpp
    src/formula/formula_scan_command.cpp
    src/formula/formula_watch_command.cpp
    src/formula/formula_strategy_support.cpp
    src/formula/formula_strategy_context.cpp
    src/formula/formula_strategy_normalize.cpp
    src/formula/formula_strategy_evaluate.cpp
    src/formula/formula_strategy_scan.cpp
    src/formula/formula_strategy_backtest.cpp
    src/formula/formula_strategy_command.cpp
    src/formula/formula_backtest.cpp
)

set(TDX_FORMULA_EXTRACTOR_SOURCES
    src/formula/formulas_extractor_catalog.cpp
    src/formula/formulas_pe.cpp
    src/formula/formulas_icon.cpp
    src/formula/formulas_extract.cpp
    src/formula/formulas_command.cpp
)

add_library(tdx_native STATIC
    src/corporate/benchmark_analysis.cpp
    src/research/active_lhb.cpp
    src/research/abnormal_details.cpp
    src/research/abnormal_moves.cpp
    src/research/announcement_signals_catalog.cpp
    src/research/announcement_signals_command.cpp
    src/research/announcement_signals_fetch.cpp
    src/research/announcement_signals_normalize.cpp
    src/research/announcement_signals_service.cpp
    src/research/announcement_signals_sort.cpp
    src/research/announcement_signals_support.cpp
    src/research/anomaly_risk_catalog.cpp
    src/research/anomaly_risk_support.cpp
    src/research/anomaly_risk_evidence.cpp
    src/research/anomaly_risk_normalize_statistics.cpp
    src/research/anomaly_risk_normalize_suspension.cpp
    src/research/anomaly_risk_query_plan.cpp
    src/research/anomaly_risk_response.cpp
    src/research/anomaly_risk_service.cpp
    src/research/anomaly_risk_command.cpp
    src/funds/active_funds.cpp
    src/market/auction.cpp
    src/trading/block_backtest.cpp
    src/trading/block_rotation.cpp
    src/trading/block_trades_support.cpp
    src/trading/block_trades_normalize_core.cpp
    src/trading/block_trades_normalize_analysis.cpp
    src/trading/block_trades_fetch.cpp
    src/trading/block_trades_service.cpp
    src/trading/block_trades_command.cpp
    src/bonds/bond_reference_catalog.cpp
    src/bonds/bond_reference_support.cpp
    src/bonds/bond_reference_normalize.cpp
    src/bonds/bond_reference_service_fetch.cpp
    src/bonds/bond_reference_service_query.cpp
    src/bonds/bond_reference_service_query_records.cpp
    src/bonds/bond_reference_service_query_projections.cpp
    src/bonds/bond_reference_service_query_result.cpp
    src/bonds/bond_reference_command.cpp
    src/calendar/calendar_support.cpp
    src/calendar/calendar_normalize.cpp
    src/calendar/calendar_service.cpp
    src/calendar/calendar_command.cpp
    src/research/capital_strength_catalog.cpp
    src/research/capital_strength_support.cpp
    src/research/capital_strength_normalize.cpp
    src/research/capital_strength_confluence.cpp
    src/research/capital_strength_sort.cpp
    src/research/capital_strength_summary.cpp
    src/research/capital_strength_query_plan.cpp
    src/research/capital_strength_service.cpp
    src/research/capital_strength_command.cpp
    src/cloud/cloud_workflow.cpp
    src/cloud/cloud_routes.cpp
    src/cloud/cloud_variants.cpp
    src/cloud/cloud_calc_service_support.cpp
    src/cloud/cloud_calc_audit.cpp
    src/cloud/cloud_calc_evaluation.cpp
    src/cloud/cloud_calc_template.cpp
    src/cloud/cloud_calc_request.cpp
    src/cloud/cloud_calc_batch.cpp
    src/cloud/cloud_calc_command.cpp
    src/cloud/cloud_calc_builtins_catalog.cpp
    src/cloud/cloud_calc_builtins_algorithms.cpp
    src/cloud/cloud_calc_builtins_execute.cpp
    src/cloud/cloud_calc_config.cpp
    src/cloud/cloud_calc_host_quote.cpp
    src/cloud/cloud_calc_host_seal.cpp
    src/cloud/cloud_calc_host_group.cpp
    src/cloud/cloud_calc_host_finance.cpp
    src/cloud/cloud_calc_host_resolve.cpp
    src/cloud/cloud_calc_interpreter.cpp
    src/cloud/cloud_resilience.cpp
    src/common/common.cpp
    src/common/time.cpp
    src/research/commodity_links_catalog.cpp
    src/research/commodity_links_support.cpp
    src/research/commodity_links_normalize_quotes.cpp
    src/research/commodity_links_normalize_themes.cpp
    src/research/commodity_links_normalize_securities.cpp
    src/research/commodity_links_service_fetch.cpp
    src/research/commodity_links_service_query.cpp
    src/research/commodity_links_command.cpp
    src/corporate/company_changes.cpp
    src/corporate/consensus.cpp
    src/bonds/convertible_bonds.cpp
    src/bonds/convertible_bonds_catalog.cpp
    src/bonds/convertible_bonds_listed.cpp
    src/bonds/convertible_bonds_issuance.cpp
    src/bonds/convertible_bonds_pricing.cpp
    src/bonds/convertible_bonds_service_fetch_master.cpp
    src/bonds/convertible_bonds_service_fetch_pricing.cpp
    src/bonds/convertible_bonds_service_query.cpp
    src/bonds/convertible_bonds_service_query_subscriptions.cpp
    src/bonds/convertible_bonds_service_query_pricing.cpp
    src/bonds/convertible_bonds_service_query_pending.cpp
    src/bonds/convertible_bonds_service_query_listed.cpp
    src/bonds/convertible_bonds_commands.cpp
    src/corporate/corporate.cpp
    src/corporate/corporate_gbbq.cpp
    src/corporate/corporate_support.cpp
    src/corporate/corporate_adjustment.cpp
    src/corporate/corporate_adjustment_cache.cpp
    src/corporate/corporate_fetch.cpp
    src/corporate/corporate_commands.cpp
    src/corporate/corporate_orders.cpp
    src/research/curated_data_support.cpp
    src/research/curated_data_catalog.cpp
    src/research/curated_data_security.cpp
    src/research/curated_data_resources.cpp
    src/research/curated_data_normalize.cpp
    src/research/curated_data_fetch.cpp
    src/research/curated_data_service.cpp
    src/research/curated_data_command.cpp
    src/market/daily.cpp
    src/market/local_kline.cpp
    src/data/disclosures_catalog.cpp
    src/data/disclosures_command_input.cpp
    src/data/disclosures_command_options.cpp
    src/data/disclosures_command_support.cpp
    src/data/disclosures_command_universe.cpp
    src/data/disclosures_command_audit.cpp
    src/data/disclosures_command_maintain.cpp
    src/data/disclosures_command_backfill.cpp
    src/data/disclosures_command_query.cpp
    src/data/disclosures_command.cpp
    src/data/disclosures_archive.cpp
    src/data/disclosures_backfill.cpp
    src/data/disclosures_normalize.cpp
    src/data/disclosures_service.cpp
    src/data/disclosures_support.cpp
    src/research/economic_indicators_support.cpp
    src/research/economic_indicators_normalize.cpp
    src/research/economic_indicators_fetch.cpp
    src/research/economic_indicators_query_support.cpp
    src/research/economic_indicators_service.cpp
    src/research/economic_indicators_command.cpp
    src/funds/etf_flows.cpp
    src/corporate/employees.cpp
    src/corporate/event_impact.cpp
    src/data/external_series.cpp
    src/data/external_signals.cpp
    src/data/local_signals.cpp
    src/data/local_signals_command.cpp
    src/funds/exchange_funds_catalog.cpp
    src/funds/exchange_funds_support.cpp
    src/funds/exchange_funds_normalize.cpp
    src/funds/exchange_funds_quotes.cpp
    src/funds/exchange_funds_fetch.cpp
    src/funds/exchange_funds_service.cpp
    src/funds/exchange_funds_command.cpp
    src/funds/fund_reference.cpp
    src/funds/fund_reference_command.cpp
    src/corporate/equity_valuation_catalog.cpp
    src/corporate/equity_valuation_support.cpp
    src/corporate/equity_valuation_projection.cpp
    src/corporate/equity_valuation_normalize_industry.cpp
    src/corporate/equity_valuation_normalize_security.cpp
    src/corporate/equity_valuation_normalize.cpp
    src/corporate/equity_valuation_request.cpp
    src/corporate/equity_valuation_service.cpp
    src/corporate/equity_valuation_command.cpp
    src/corporate/equity_performance.cpp
    src/exchange/exchange_supervision.cpp
    src/research/factors.cpp
    src/research/factors_support.cpp
    src/research/factors_normalize.cpp
    src/research/factors_breadth.cpp
    src/research/factors_snapshot.cpp
    src/research/factors_commands.cpp
    src/research/factors_query_support.cpp
    src/research/factors_query_matrix.cpp
    src/research/factors_query_overview.cpp
    src/research/factors_query_security.cpp
    src/research/factors_query_table.cpp
    src/corporate/financial_screen.cpp
    src/corporate/financial_insights_catalog.cpp
    src/corporate/financial_insights_support.cpp
    src/corporate/financial_insights_normalize.cpp
    src/corporate/financial_insights_service_fetch.cpp
    src/corporate/financial_insights_service_query.cpp
    src/corporate/financial_insights_command.cpp
    src/corporate/finance_eligibility.cpp
    src/corporate/finance_events.cpp
    src/research/flow_followup_support.cpp
    src/research/flow_followup_history.cpp
    src/research/flow_followup_model.cpp
    src/research/flow_followup_source.cpp
    src/research/flow_followup_service.cpp
    src/research/flow_followup_command.cpp
    src/common/json.cpp
    src/market/minute.cpp
    src/market/minute_download.cpp
    src/market/minute_download_support.cpp
    src/market/minute_download_kline.cpp
    src/market/minute_download_expansion.cpp
    src/market/minute_download_commands.cpp
    src/derivatives/options.cpp
    src/derivatives/options_expiry.cpp
    src/derivatives/options_pricing.cpp
    src/derivatives/options_volatility.cpp
    src/derivatives/options_chain.cpp
    src/derivatives/options_commands.cpp
    src/corporate/ownership.cpp
    src/corporate/ownership_support.cpp
    src/corporate/ownership_changes.cpp
    src/corporate/ownership_pledges.cpp
    src/corporate/ownership_fetch.cpp
    src/corporate/ownership_commands.cpp
    src/corporate/overview_factors.cpp
    src/corporate/patent_statistics.cpp
    ${TDX_RECON_SOURCES}
    src/research/relative_valuation_catalog.cpp
    src/research/relative_valuation_support.cpp
    src/research/relative_valuation_projection.cpp
    src/research/relative_valuation_normalize.cpp
    src/research/relative_valuation_query_plan.cpp
    src/research/relative_valuation_fetch.cpp
    src/research/relative_valuation_service.cpp
    src/research/relative_valuation_command.cpp
    src/corporate/repurchases_catalog.cpp
    src/corporate/repurchases_support.cpp
    src/corporate/repurchases_normalize_plans.cpp
    src/corporate/repurchases_normalize_statistics.cpp
    src/corporate/repurchases_normalize_hk.cpp
    src/corporate/repurchases_service_fetch.cpp
    src/corporate/repurchases_service_query.cpp
    src/corporate/repurchases_command.cpp
    src/research/reverse_repo_catalog.cpp
    src/research/reverse_repo_support.cpp
    src/research/reverse_repo_normalize.cpp
    src/research/reverse_repo_sort.cpp
    src/research/reverse_repo_summary.cpp
    src/research/reverse_repo_query_plan.cpp
    src/research/reverse_repo_fetch.cpp
    src/research/reverse_repo_service.cpp
    src/research/reverse_repo_command.cpp
    src/research/research_catalog.cpp
    src/research/research_support.cpp
    src/research/research_normalize_master.cpp
    src/research/research_normalize_detail.cpp
    src/research/research_service_fetch.cpp
    src/research/research_service_query.cpp
    src/research/research_command.cpp
    src/corporate/roadshows.cpp
    src/data/security_directory.cpp
    src/data/security_status.cpp
    src/market/seal_order.cpp
    src/market/transport.cpp
    src/cloud/pbrpc.cpp
    src/protocol/professional_data_support.cpp
    src/protocol/professional_data_resource.cpp
    src/protocol/professional_data_zip.cpp
    src/protocol/professional_data_catalog.cpp
    src/protocol/professional_data_parse.cpp
    src/protocol/professional_data_fetch.cpp
    src/protocol/professional_data_document.cpp
    src/protocol/professional_data_command.cpp
    src/corporate/profit_gaps.cpp
    src/cloud/tqlex_config_text.cpp
    src/cloud/tqlex_response.cpp
    src/cloud/tqlex_catalog.cpp
    src/cloud/tqlex_request.cpp
    src/cloud/tqlex_transport.cpp
    src/cloud/tqlex_execute.cpp
    src/cloud/tqlex_command.cpp
    src/exchange/total_return_gap.cpp
    src/cloud/tpool.cpp
    src/cloud/tpool_flow_support.cpp
    src/cloud/tpool_flow_schedule.cpp
    src/cloud/tpool_flow_projection.cpp
    src/cloud/tpool_flow_state.cpp
    src/cloud/tpool_flow_diff.cpp
    src/cloud/tpool_rule.cpp
    src/cloud/tpool_ranking.cpp
    src/cloud/tpool_builtin_fields.cpp
    src/cloud/tpool_callbacks.cpp
    src/cloud/tpool_history.cpp
    src/cloud/tpool_history_text.cpp
    src/cloud/tpool_history_export.cpp
    src/cloud/tpool_history_command.cpp
    src/cloud/tpool_actions.cpp
    src/cloud/tpool_evaluate.cpp
    src/cloud/tpool_commands.cpp
    src/trading/trades_support.cpp
    src/trading/trades_records.cpp
    src/trading/trades_protocol.cpp
    src/trading/trades_aggregate.cpp
    src/trading/trades_project.cpp
    src/trading/trades_fetch.cpp
    src/trading/trades_command.cpp
    src/trading/investment_parse.cpp
    src/trading/investment_holdings.cpp
    src/trading/investment_fees.cpp
    src/trading/investment_valuation.cpp
    src/trading/investment_query.cpp
    src/trading/investment_command.cpp
    src/corporate/unlocks_support.cpp
    src/corporate/unlocks_normalize.cpp
    src/corporate/unlocks_fetch.cpp
    src/corporate/unlocks_query.cpp
    src/corporate/unlocks_command.cpp
    src/market/valuation.cpp
    src/data/jsn.cpp
    src/data/jsn_variants.cpp
    src/data/jsn_variants_catalog.cpp
    src/data/jsn_variants_scan.cpp
    src/data/jsn_variants_coverage.cpp
    src/data/jsn_variants_discovery.cpp
    src/data/jsn_variants_candidates.cpp
    src/data/jsn_variants_commands.cpp
    src/data/blocks_support.cpp
    src/data/blocks_parse.cpp
    src/data/blocks_business.cpp
    src/data/security_metadata.cpp
    src/data/blocks_custom.cpp
    src/data/blocks_loader.cpp
    src/data/blocks_quote.cpp
    src/data/blocks_command.cpp
    ${TDX_FORMULA_SOURCES}
    src/industry/hk_events_catalog.cpp
    src/industry/hk_events_support.cpp
    src/industry/hk_events_normalize.cpp
    src/industry/hk_events_short_history.cpp
    src/industry/hk_events_service_fetch.cpp
    src/industry/hk_events_service_query.cpp
    src/industry/hk_events_command.cpp
    src/common/blowfish_catalog.cpp
    src/common/blowfish.cpp
    src/industry/hk_actions_blowfish.cpp
    src/industry/hk_actions_parse.cpp
    src/industry/hk_actions_query.cpp
    src/industry/hk_actions_adjustment.cpp
    src/industry/hk_actions_command.cpp
    src/industry/hk_finance_parse.cpp
    src/industry/hk_finance_query.cpp
    src/industry/hk_finance_command.cpp
    src/institution/foreign_alerts.cpp
    src/corporate/forecasts_catalog.cpp
    src/corporate/forecasts_support.cpp
    src/corporate/forecasts_normalize_industry.cpp
    src/corporate/forecasts_normalize_security.cpp
    src/corporate/forecasts_normalize_hong_kong.cpp
    src/corporate/forecasts_summary.cpp
    src/corporate/forecasts_service_fetch.cpp
    src/corporate/forecasts_service_query.cpp
    src/corporate/forecasts_command.cpp
    src/funds/funds_catalog.cpp
    src/funds/funds_support.cpp
    src/funds/funds_normalize.cpp
    src/funds/funds_industries.cpp
    src/funds/funds_fetch.cpp
    src/funds/funds_build_master.cpp
    src/funds/funds_build_expanded.cpp
    src/funds/funds_service.cpp
    src/funds/funds_command.cpp
    src/funds/fund_analytics_catalog.cpp
    src/funds/fund_analytics_support.cpp
    src/funds/fund_analytics_normalize_performance.cpp
    src/funds/fund_analytics_normalize_holdings.cpp
    src/funds/fund_analytics_normalize_position.cpp
    src/funds/fund_analytics_normalize.cpp
    src/funds/fund_analytics_service.cpp
    src/funds/fund_analytics_command.cpp
    src/funds/fund_statistics.cpp
    src/funds/fund_calendar.cpp
    src/derivatives/futures_issuance_support.cpp
    src/derivatives/futures_issuance_futures.cpp
    src/derivatives/futures_issuance_ipo.cpp
    src/derivatives/futures_issuance_capital.cpp
    src/derivatives/futures_issuance_service.cpp
    src/derivatives/futures_issuance_command.cpp
    src/corporate/gdr.cpp
    src/corporate/global_performance.cpp
    src/common/http.cpp
    src/market/hyzt.cpp
    src/data/image_data.cpp
    src/institution/institution_cache.cpp
    src/institution/institution_support.cpp
    src/institution/institution_security_normalize.cpp
    src/institution/institution_holder_normalize.cpp
    src/institution/institution_security_service.cpp
    src/institution/institution_holder_service.cpp
    src/institution/institution_command.cpp
    src/institution/institution_analysis_catalog.cpp
    src/institution/institution_analysis_support.cpp
    src/institution/institution_analysis_normalize.cpp
    src/institution/institution_analysis_summary.cpp
    src/institution/institution_analysis_compose.cpp
    src/institution/institution_analysis_service.cpp
    src/institution/institution_analysis_command.cpp
    src/institution/institution_lhb.cpp
    src/industry/industry_profile_catalog.cpp
    src/industry/industry_profile_support.cpp
    src/industry/industry_profile_normalize_holdings.cpp
    src/industry/industry_profile_normalize_shareholders.cpp
    src/industry/industry_profile_topology.cpp
    src/industry/industry_profile_service_fetch.cpp
    src/industry/industry_profile_service_query.cpp
    src/industry/industry_profile_command.cpp
    src/research/intelligence_normalize.cpp
    src/research/intelligence_support.cpp
    src/research/intelligence_graph.cpp
    src/research/intelligence_service.cpp
    src/research/intelligence_command.cpp
    src/exchange/index_volatility.cpp
    src/data/jsn_data.cpp
    src/leverage/leverage_support.cpp
    src/leverage/leverage_margin_normalize.cpp
    src/leverage/leverage_stock_connect_normalize.cpp
    src/leverage/leverage_margin_service.cpp
    src/leverage/leverage_stock_connect_service.cpp
    src/leverage/leverage_command.cpp
    src/market/level2.cpp
    src/market/level2_tpbus_115.cpp
    src/market/level2_tpbus_115_command.cpp
    src/market/level2_sdk_quote_transition_command.cpp
    src/market/level2_sdk_180x_host_projection_command.cpp
    src/market/level2_sdk_1803_record_projection_command.cpp
    src/market/level2_sdk_correlation_transition_command.cpp
    src/market/level2_sdk_1804_host_projection_command.cpp
    src/market/level2_sdk_fixed_snapshot_projection_command.cpp
    src/market/level2_sdk_dual_snapshot_transition_command.cpp
    src/market/level2_sdk_4655_companion_raw_transition_command.cpp
    src/market/pe_export_reader.cpp
    src/market/level2_sdk_compatibility_preflight.cpp
    src/market/level2_sdk_compatibility_preflight_command.cpp
    src/market/level2_tpbus_4650_dispatch_preflight.cpp
    src/market/level2_tpbus_4650_price_primitives.cpp
    src/market/level2_tpbus_4650_dispatch_preflight_command.cpp
    src/market/level2_sdk_1807_plan.cpp
    src/market/level2_sdk_fnreqdata_plan.cpp
    src/market/level2_sdk_fnreqdata_18031_plan.cpp
    src/market/level2_sdk_fnsubscribe_batch_plan.cpp
      src/market/level2_sdk_callback_route_plan.cpp
      src/market/level2_sdk_callback_invocation.cpp
      src/market/level2_sdk_quote_transition.cpp
      src/market/level2_sdk_correlation_transition.cpp
      src/market/level2_sdk_security_classifier.cpp
      src/market/level2_sdk_180x_host_projection.cpp
      src/market/level2_sdk_1803_depth_record_projection.cpp
      src/market/level2_sdk_18031_queue_record_projection.cpp
      src/market/level2_sdk_1804_host_projection.cpp
      src/market/level2_sdk_fixed_snapshot_projection.cpp
      src/market/level2_sdk_snapshot_digest.cpp
      src/market/level2_sdk_4654_dual_snapshot_transition.cpp
      src/market/level2_sdk_4651_dual_snapshot_transition.cpp
      src/market/level2_sdk_4655_dual_snapshot_transition.cpp
      src/market/level2_fasthq_subscribe_plan.cpp
    src/market/level2_sdk_correlation.cpp
    src/market/level2_tdxw_ipc_requests.cpp
    src/market/level2_sdk_ticks.cpp
    src/market/level2_sdk_json_4653.cpp
    src/market/level2_sdk_json_adapter.cpp
    src/market/level2_tcalc_order_flow.cpp
    src/market/level2_formula_context.cpp
    src/research/lhb.cpp
    src/research/limit_quality.cpp
    src/research/limit_ladder.cpp
    src/research/limit_review_catalog.cpp
    src/research/limit_review_support.cpp
    src/research/limit_review_normalize_current.cpp
    src/research/limit_review_normalize_history.cpp
    src/research/limit_review_normalize_daily.cpp
    src/research/limit_review_query_plan.cpp
    src/research/limit_review_fetch.cpp
    src/research/limit_review_service.cpp
    src/research/limit_review_command.cpp
    src/market/market_protocol.cpp
    src/market/market_support.cpp
    src/market/market_document.cpp
    src/market/market_transport.cpp
    src/market/market_service.cpp
    src/market/market_session.cpp
    src/market/market_command.cpp
    src/market/market_watch_command.cpp
    src/market/market_stream.cpp
    src/market/market_cage.cpp
    src/market/panorama.cpp
    src/market/ranking.cpp
    src/corporate/recent_watch.cpp
    src/institution/ratings_support.cpp
    src/institution/ratings_names.cpp
    src/institution/ratings_summary.cpp
    src/institution/ratings_normalize.cpp
    src/institution/ratings_fetch.cpp
    src/institution/ratings_query.cpp
    src/institution/ratings_command.cpp
    src/registry/registry.cpp
    src/registry/registry_cloud_gateway.cpp
    src/registry/registry_formula.cpp
    src/registry/registry_market_anomaly.cpp
    src/registry/registry_market_corporate.cpp
    src/registry/registry_market_disclosure.cpp
    src/registry/registry_market_quote.cpp
    src/registry/registry_market_reference.cpp
    src/registry/registry_market_local.cpp
    src/registry/registry_market_session.cpp
    src/registry/registry_market_valuation.cpp
    src/registry/registry_static_resource.cpp
    src/registry/registry_system.cpp
    src/market/session_endpoint_roles.cpp
    src/market/session_audit.cpp
    src/market/session_turnover.cpp
    src/protocol/ttplugin_redirect.cpp
    src/protocol/ttplugin_servers.cpp
    src/research/shareholder_signals_catalog.cpp
    src/research/shareholder_signals_support.cpp
    src/research/shareholder_signals_normalize.cpp
    src/research/shareholder_signals_investor.cpp
    src/research/shareholder_signals_query_plan.cpp
    src/research/shareholder_signals_fetch.cpp
    src/research/shareholder_signals_investor_projection.cpp
    src/research/shareholder_signals_service.cpp
    src/research/shareholder_signals_command.cpp
    ${TDX_SERVER_SOURCES}
    src/research/special_attention.cpp
    src/corporate/specialized_metrics.cpp
    src/research/special_situations_catalog.cpp
    src/research/special_situations_support.cpp
    src/research/special_situations_normalize.cpp
    src/research/special_situations_quotes.cpp
    src/research/special_situations_service_fetch.cpp
    src/research/special_situations_service_query.cpp
    src/research/special_situations_command.cpp
    src/research/state_owned_reform_catalog.cpp
    src/research/state_owned_reform_support.cpp
    src/research/state_owned_reform_sort.cpp
    src/research/state_owned_reform_normalize_groups.cpp
    src/research/state_owned_reform_normalize_records.cpp
    src/research/state_owned_reform_service_fetch.cpp
    src/research/state_owned_reform_service_query.cpp
    src/research/state_owned_reform_response.cpp
    src/research/state_owned_reform_command.cpp
    src/research/strategic_themes_catalog.cpp
    src/research/strategic_themes_support.cpp
    src/research/strategic_themes_normalize_master.cpp
    src/research/strategic_themes_normalize_detail.cpp
    src/research/strategic_themes_query_plan.cpp
    src/research/strategic_themes_fetch_master.cpp
    src/research/strategic_themes_fetch_detail.cpp
    src/research/strategic_themes_service.cpp
    src/research/strategic_themes_command.cpp
    src/research/theme_library_support.cpp
    src/research/theme_library_catalog.cpp
    src/research/theme_library_normalize.cpp
    src/research/theme_library_fetch.cpp
    src/research/theme_library_service.cpp
    src/research/theme_library_command.cpp
    src/research/stats_support.cpp
    src/research/stats_parse.cpp
    src/research/stats_archive.cpp
    src/research/stats_document.cpp
    src/research/stats_service.cpp
    src/research/stats_command.cpp
    src/research/strong_stocks_catalog.cpp
    src/research/strong_stocks_support.cpp
    src/research/strong_stocks_summary.cpp
    src/research/strong_stocks_normalize.cpp
    src/research/strong_stocks_sort.cpp
    src/research/strong_stocks_service.cpp
    src/research/strong_stocks_command.cpp
    src/research/technical_signals.cpp
    src/research/technical_signals_support.cpp
    src/research/technical_signals_normalize.cpp
    src/research/technical_signals_snapshot.cpp
    src/research/technical_signals_security.cpp
    src/research/technical_signals_commands.cpp
    src/research/thematic_opportunities_catalog.cpp
    src/research/thematic_opportunities_support.cpp
    src/research/thematic_opportunities_normalize_groups.cpp
    src/research/thematic_opportunities_normalize_hype.cpp
    src/research/thematic_opportunities_service_fetch.cpp
    src/research/thematic_opportunities_service_query.cpp
    src/research/thematic_opportunities_command.cpp
    src/research/hot_history.cpp
    src/research/hot_history_command.cpp
    src/research/historical_securities.cpp
    src/research/historical_securities_command.cpp
    src/research/index_events.cpp
    src/research/index_events_command.cpp
    src/research/shape_match_parse.cpp
    src/research/shape_match_score.cpp
    src/research/shape_match_scan.cpp
    src/research/shape_match_query.cpp
    src/research/shape_match_command.cpp
    src/research/threshold_stocks_catalog.cpp
    src/research/threshold_stocks_support.cpp
    src/research/threshold_stocks_normalize_history.cpp
    src/research/threshold_stocks_normalize_members.cpp
    src/research/threshold_stocks_sort.cpp
    src/research/threshold_stocks_summary.cpp
    src/research/threshold_stocks_query_plan.cpp
    src/research/threshold_stocks_fetch.cpp
    src/research/threshold_stocks_service.cpp
    src/research/threshold_stocks_command.cpp
    src/corporate/tender_offers.cpp
)
target_include_directories(tdx_native
    PUBLIC include
    PRIVATE
        src/bonds
        src/calendar
        src/cloud
        src/common
        src/corporate
        src/data
        src/derivatives
        src/exchange
        src/formula
        src/funds
        src/industry
        src/institution
        src/leverage
        src/market
        src/protocol
        src/recon
        src/registry
        src/research
        src/server
        src/trading
)
target_compile_definitions(tdx_native PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(tdx_native PRIVATE
    TDX_FORMULA_BUNDLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/formula"
    TDX_RECON_ASSET_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/recon")
if(MSVC)
    target_compile_options(tdx_native PRIVATE /W4)
else()
    target_compile_options(tdx_native PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    target_link_libraries(tdx_native PUBLIC bcrypt ws2_32 winhttp iphlpapi)
endif()
target_link_libraries(tdx_native PUBLIC ZLIB::ZLIB)

add_executable(tdx-tool src/server/main.cpp)
target_link_libraries(tdx-tool PRIVATE tdx_native)
set(TDX_FORMULA_ASSET_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/bundle-manifest.json
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/formulas.json
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/drawicon-manifest.json
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/drawicon-strip.png
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/drawicon-strip.bmp)
set(TDX_FORMULA_ASSET_STAMP
    ${CMAKE_CURRENT_BINARY_DIR}/formula-assets/.bundle-stamp)
add_custom_command(OUTPUT ${TDX_FORMULA_ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula
            ${CMAKE_CURRENT_BINARY_DIR}/formula-assets
    COMMAND ${CMAKE_COMMAND} -E touch ${TDX_FORMULA_ASSET_STAMP}
    DEPENDS ${TDX_FORMULA_ASSET_FILES}
    COMMENT "Copying the standalone formula bundle")
add_custom_target(tdx-formula-assets DEPENDS ${TDX_FORMULA_ASSET_STAMP})
set(TDX_RECON_ASSET_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/recon/api-contract-post-requests.json)
set(TDX_RECON_ASSET_STAMP
    ${CMAKE_CURRENT_BINARY_DIR}/recon-assets/.bundle-stamp)
add_custom_command(OUTPUT ${TDX_RECON_ASSET_STAMP}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/recon
            ${CMAKE_CURRENT_BINARY_DIR}/recon-assets
    COMMAND ${CMAKE_COMMAND} -E touch ${TDX_RECON_ASSET_STAMP}
    DEPENDS ${TDX_RECON_ASSET_FILES}
    COMMENT "Copying the recon request catalog")
add_custom_target(tdx-recon-assets DEPENDS ${TDX_RECON_ASSET_STAMP})
add_dependencies(tdx-tool tdx-formula-assets tdx-recon-assets)
if(MINGW)
    target_link_options(tdx-tool PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

if(TDX_BUILD_TCALC_EXTRACTOR)
    add_library(tdx_formula_extractor STATIC ${TDX_FORMULA_EXTRACTOR_SOURCES})
    target_link_libraries(tdx_formula_extractor PUBLIC tdx_native)
    target_include_directories(tdx_formula_extractor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
    if(MSVC)
        target_compile_options(tdx_formula_extractor PRIVATE /W4)
    else()
        target_compile_options(tdx_formula_extractor PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_executable(tdx-formula-extractor
        src/formula/formulas_extractor_main.cpp)
    target_link_libraries(tdx-formula-extractor PRIVATE tdx_formula_extractor)
    add_dependencies(tdx-formula-extractor tdx-formula-assets)
    if(MINGW)
        target_link_options(tdx-formula-extractor PRIVATE
            -municode -static -static-libgcc -static-libstdc++)
    endif()
endif()

enable_testing()
add_executable(tdx-daily-tests tests/daily_tests.cpp)
target_link_libraries(tdx-daily-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-daily-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-daily-tests COMMAND tdx-daily-tests)

add_executable(tdx-native-tests tests/native_tests.cpp)
target_link_libraries(tdx-native-tests PRIVATE tdx_native)
target_include_directories(tdx-native-tests PRIVATE src/market)
if(MINGW)
    target_link_options(tdx-native-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-native-tests COMMAND tdx-native-tests)

add_executable(tdx-research-tests tests/research_tests.cpp)
target_link_libraries(tdx-research-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-research-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-research-tests COMMAND tdx-research-tests)

add_executable(tdx-tqlex-tests tests/tqlex_tests.cpp)
target_link_libraries(tdx-tqlex-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-tqlex-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-tqlex-tests COMMAND tdx-tqlex-tests)

add_executable(tdx-industry-profile-tests tests/industry_profile_tests.cpp)
target_link_libraries(tdx-industry-profile-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-industry-profile-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-industry-profile-tests COMMAND tdx-industry-profile-tests)

add_executable(tdx-block-trades-tests tests/block_trades_tests.cpp)
target_link_libraries(tdx-block-trades-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-block-trades-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-block-trades-tests COMMAND tdx-block-trades-tests)

add_executable(tdx-session-audit-tests tests/session_audit_tests.cpp)
target_link_libraries(tdx-session-audit-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-session-audit-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-session-audit-tests COMMAND tdx-session-audit-tests)

add_executable(tdx-ttplugin-redirect-tests tests/ttplugin_redirect_tests.cpp)
target_link_libraries(tdx-ttplugin-redirect-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-ttplugin-redirect-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-ttplugin-redirect-tests COMMAND tdx-ttplugin-redirect-tests)

add_executable(tdx-ttplugin-servers-tests tests/ttplugin_servers_tests.cpp)
target_link_libraries(tdx-ttplugin-servers-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-ttplugin-servers-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-ttplugin-servers-tests COMMAND tdx-ttplugin-servers-tests)

add_executable(tdx-options-tests tests/options_tests.cpp)
target_link_libraries(tdx-options-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-options-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-options-tests COMMAND tdx-options-tests)

add_executable(tdx-professional-data-tests tests/professional_data_tests.cpp)
target_link_libraries(tdx-professional-data-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-professional-data-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-professional-data-tests COMMAND tdx-professional-data-tests)

add_executable(tdx-finance-events-tests tests/finance_events_tests.cpp)
target_link_libraries(tdx-finance-events-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-finance-events-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-finance-events-tests COMMAND tdx-finance-events-tests)

add_executable(tdx-finance-eligibility-tests tests/finance_eligibility_tests.cpp)
target_link_libraries(tdx-finance-eligibility-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-finance-eligibility-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-finance-eligibility-tests COMMAND tdx-finance-eligibility-tests)

add_executable(tdx-market-cage-tests tests/market_cage_tests.cpp)
target_link_libraries(tdx-market-cage-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-market-cage-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-market-cage-tests COMMAND tdx-market-cage-tests)

add_executable(tdx-repurchases-tests tests/repurchases_tests.cpp)
target_link_libraries(tdx-repurchases-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-repurchases-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-repurchases-tests COMMAND tdx-repurchases-tests)

add_executable(tdx-tender-offers-tests tests/tender_offers_tests.cpp)
target_link_libraries(tdx-tender-offers-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-tender-offers-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-tender-offers-tests COMMAND tdx-tender-offers-tests)

add_executable(tdx-ownership-tests tests/ownership_tests.cpp)
target_link_libraries(tdx-ownership-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-ownership-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-ownership-tests COMMAND tdx-ownership-tests)

add_executable(tdx-forecasts-tests tests/forecasts_tests.cpp)
target_link_libraries(tdx-forecasts-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-forecasts-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-forecasts-tests COMMAND tdx-forecasts-tests)

add_executable(tdx-disclosures-tests tests/disclosures_tests.cpp)
target_link_libraries(tdx-disclosures-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-disclosures-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-disclosures-tests COMMAND tdx-disclosures-tests)

add_executable(tdx-active-funds-tests tests/active_funds_tests.cpp)
target_link_libraries(tdx-active-funds-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-active-funds-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-active-funds-tests COMMAND tdx-active-funds-tests)

add_executable(tdx-etf-flows-tests tests/etf_flows_tests.cpp)
target_link_libraries(tdx-etf-flows-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-etf-flows-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-etf-flows-tests COMMAND tdx-etf-flows-tests)

add_executable(tdx-security-directory-tests tests/security_directory_tests.cpp)
target_link_libraries(tdx-security-directory-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-security-directory-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-security-directory-tests COMMAND tdx-security-directory-tests)

add_executable(tdx-institution-lhb-tests tests/institution_lhb_tests.cpp)
target_link_libraries(tdx-institution-lhb-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-institution-lhb-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-institution-lhb-tests COMMAND tdx-institution-lhb-tests)

add_executable(tdx-ratings-tests tests/ratings_tests.cpp)
target_link_libraries(tdx-ratings-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-ratings-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-ratings-tests COMMAND tdx-ratings-tests)

add_executable(tdx-foreign-alerts-tests tests/foreign_alerts_tests.cpp)
target_link_libraries(tdx-foreign-alerts-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-foreign-alerts-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-foreign-alerts-tests COMMAND tdx-foreign-alerts-tests)

add_executable(tdx-corporate-tests tests/corporate_tests.cpp)
target_link_libraries(tdx-corporate-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-corporate-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-corporate-tests COMMAND tdx-corporate-tests)

add_executable(tdx-limit-quality-tests tests/limit_quality_tests.cpp)
target_link_libraries(tdx-limit-quality-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-limit-quality-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-limit-quality-tests COMMAND tdx-limit-quality-tests)

add_executable(tdx-limit-review-tests tests/limit_review_tests.cpp)
target_link_libraries(tdx-limit-review-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-limit-review-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-limit-review-tests COMMAND tdx-limit-review-tests)

add_executable(tdx-session-turnover-tests tests/session_turnover_tests.cpp)
target_link_libraries(tdx-session-turnover-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-session-turnover-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-session-turnover-tests COMMAND tdx-session-turnover-tests)

add_executable(tdx-convertible-bonds-tests tests/convertible_bonds_tests.cpp)
target_link_libraries(tdx-convertible-bonds-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-convertible-bonds-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-convertible-bonds-tests COMMAND tdx-convertible-bonds-tests)

add_executable(tdx-block-rotation-tests tests/block_rotation_tests.cpp)
target_link_libraries(tdx-block-rotation-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-block-rotation-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-block-rotation-tests COMMAND tdx-block-rotation-tests)

add_executable(tdx-blocks-tests tests/blocks_tests.cpp)
target_link_libraries(tdx-blocks-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-blocks-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-blocks-tests COMMAND tdx-blocks-tests)

add_executable(tdx-limit-ladder-tests tests/limit_ladder_tests.cpp)
target_link_libraries(tdx-limit-ladder-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-limit-ladder-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-limit-ladder-tests COMMAND tdx-limit-ladder-tests)

add_executable(tdx-threshold-stocks-tests tests/threshold_stocks_tests.cpp)
target_link_libraries(tdx-threshold-stocks-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-threshold-stocks-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-threshold-stocks-tests COMMAND tdx-threshold-stocks-tests)

add_executable(tdx-capital-strength-tests tests/capital_strength_tests.cpp)
target_link_libraries(tdx-capital-strength-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-capital-strength-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-capital-strength-tests COMMAND tdx-capital-strength-tests)

add_executable(tdx-strong-stocks-tests tests/strong_stocks_tests.cpp)
target_link_libraries(tdx-strong-stocks-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-strong-stocks-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-strong-stocks-tests COMMAND tdx-strong-stocks-tests)

add_executable(tdx-commodity-links-tests tests/commodity_links_tests.cpp)
target_link_libraries(tdx-commodity-links-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-commodity-links-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-commodity-links-tests COMMAND tdx-commodity-links-tests)

add_executable(tdx-announcement-signals-tests tests/announcement_signals_tests.cpp)
target_link_libraries(tdx-announcement-signals-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-announcement-signals-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-announcement-signals-tests COMMAND tdx-announcement-signals-tests)

add_executable(tdx-reverse-repo-tests tests/reverse_repo_tests.cpp)
target_link_libraries(tdx-reverse-repo-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-reverse-repo-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-reverse-repo-tests COMMAND tdx-reverse-repo-tests)

add_executable(tdx-exchange-supervision-tests tests/exchange_supervision_tests.cpp)
target_link_libraries(tdx-exchange-supervision-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-exchange-supervision-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-exchange-supervision-tests COMMAND tdx-exchange-supervision-tests)

add_executable(tdx-active-lhb-tests tests/active_lhb_tests.cpp)
target_link_libraries(tdx-active-lhb-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-active-lhb-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-active-lhb-tests COMMAND tdx-active-lhb-tests)

add_executable(tdx-state-owned-reform-tests tests/state_owned_reform_tests.cpp)
target_link_libraries(tdx-state-owned-reform-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-state-owned-reform-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-state-owned-reform-tests COMMAND tdx-state-owned-reform-tests)

add_executable(tdx-economic-indicators-tests tests/economic_indicators_tests.cpp)
target_link_libraries(tdx-economic-indicators-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-economic-indicators-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-economic-indicators-tests COMMAND tdx-economic-indicators-tests)

add_executable(tdx-strategic-themes-tests tests/strategic_themes_tests.cpp)
target_link_libraries(tdx-strategic-themes-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-strategic-themes-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-strategic-themes-tests COMMAND tdx-strategic-themes-tests)

add_executable(tdx-theme-library-tests tests/theme_library_tests.cpp)
target_link_libraries(tdx-theme-library-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-theme-library-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-theme-library-tests COMMAND tdx-theme-library-tests)

add_executable(tdx-bond-reference-tests tests/bond_reference_tests.cpp)
target_link_libraries(tdx-bond-reference-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-bond-reference-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-bond-reference-tests COMMAND tdx-bond-reference-tests)

add_executable(tdx-thematic-opportunities-tests tests/thematic_opportunities_tests.cpp)
target_link_libraries(tdx-thematic-opportunities-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-thematic-opportunities-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-thematic-opportunities-tests COMMAND tdx-thematic-opportunities-tests)

add_executable(tdx-hot-history-tests tests/hot_history_tests.cpp)
target_link_libraries(tdx-hot-history-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-hot-history-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-hot-history-tests COMMAND tdx-hot-history-tests)

add_executable(tdx-historical-securities-tests tests/historical_securities_tests.cpp)
target_link_libraries(tdx-historical-securities-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-historical-securities-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-historical-securities-tests COMMAND tdx-historical-securities-tests)

add_executable(tdx-index-events-tests tests/index_events_tests.cpp)
target_link_libraries(tdx-index-events-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-index-events-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-index-events-tests COMMAND tdx-index-events-tests)

add_executable(tdx-leverage-tests tests/leverage_tests.cpp)
target_link_libraries(tdx-leverage-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-leverage-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-leverage-tests COMMAND tdx-leverage-tests)

add_executable(tdx-intelligence-tests tests/intelligence_tests.cpp)
target_link_libraries(tdx-intelligence-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-intelligence-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-intelligence-tests COMMAND tdx-intelligence-tests)

add_executable(tdx-technical-signals-tests tests/technical_signals_tests.cpp)
target_link_libraries(tdx-technical-signals-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-technical-signals-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-technical-signals-tests COMMAND tdx-technical-signals-tests)

add_executable(tdx-block-backtest-tests tests/block_backtest_tests.cpp)
target_link_libraries(tdx-block-backtest-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-block-backtest-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-block-backtest-tests COMMAND tdx-block-backtest-tests)

add_executable(tdx-equity-valuation-tests tests/equity_valuation_tests.cpp)
target_link_libraries(tdx-equity-valuation-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-equity-valuation-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-equity-valuation-tests COMMAND tdx-equity-valuation-tests)

add_executable(tdx-flow-followup-tests tests/flow_followup_tests.cpp)
target_link_libraries(tdx-flow-followup-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-flow-followup-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-flow-followup-tests COMMAND tdx-flow-followup-tests)

add_executable(tdx-abnormal-moves-tests tests/abnormal_moves_tests.cpp)
target_link_libraries(tdx-abnormal-moves-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-abnormal-moves-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-abnormal-moves-tests COMMAND tdx-abnormal-moves-tests)

add_executable(tdx-abnormal-details-tests tests/abnormal_details_tests.cpp)
target_link_libraries(tdx-abnormal-details-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-abnormal-details-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-abnormal-details-tests COMMAND tdx-abnormal-details-tests)

add_executable(tdx-anomaly-risk-tests tests/anomaly_risk_tests.cpp)
target_link_libraries(tdx-anomaly-risk-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-anomaly-risk-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-anomaly-risk-tests COMMAND tdx-anomaly-risk-tests)

add_executable(tdx-profit-gaps-tests tests/profit_gaps_tests.cpp)
target_link_libraries(tdx-profit-gaps-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-profit-gaps-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-profit-gaps-tests COMMAND tdx-profit-gaps-tests)

add_executable(tdx-index-volatility-tests tests/index_volatility_tests.cpp)
target_link_libraries(tdx-index-volatility-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-index-volatility-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-index-volatility-tests COMMAND tdx-index-volatility-tests)

add_executable(tdx-total-return-gap-tests tests/total_return_gap_tests.cpp)
target_link_libraries(tdx-total-return-gap-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-total-return-gap-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-total-return-gap-tests COMMAND tdx-total-return-gap-tests)

add_executable(tdx-fund-analytics-tests tests/fund_analytics_tests.cpp)
target_link_libraries(tdx-fund-analytics-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-fund-analytics-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-fund-analytics-tests COMMAND tdx-fund-analytics-tests)

add_executable(tdx-recon-contract-tests
    tests/recon_contract_tests.cpp
    tests/recon_contract_test_support.cpp
    tests/recon_contract_formula_evaluation_tests.cpp
    tests/recon_contract_formula_workflow_tests.cpp
    tests/recon_contract_market_core_tests.cpp
    tests/recon_contract_research_signals_tests.cpp
    tests/recon_contract_realtime_corporate_tests.cpp
    tests/recon_contract_curated_issuance_tests.cpp
    tests/recon_contract_calendar_resilience_tests.cpp
)
target_link_libraries(tdx-recon-contract-tests PRIVATE tdx_native)
add_dependencies(tdx-recon-contract-tests tdx-recon-assets)
if(MINGW)
    target_link_options(tdx-recon-contract-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-recon-contract-tests COMMAND tdx-recon-contract-tests)

add_executable(tdx-recon-request-catalog-tests
    tests/recon_contract_request_catalog_tests.cpp)
target_link_libraries(tdx-recon-request-catalog-tests PRIVATE tdx_native)
target_include_directories(tdx-recon-request-catalog-tests PRIVATE src/recon)
add_dependencies(tdx-recon-request-catalog-tests tdx-recon-assets)
if(MINGW)
    target_link_options(tdx-recon-request-catalog-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-recon-request-catalog-tests
         COMMAND tdx-recon-request-catalog-tests)

add_executable(tdx-runtime-topology-tests tests/runtime_topology_tests.cpp)
target_link_libraries(tdx-runtime-topology-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-runtime-topology-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-runtime-topology-tests COMMAND tdx-runtime-topology-tests)

add_executable(tdx-funds-resilience-tests tests/funds_resilience_tests.cpp)
target_link_libraries(tdx-funds-resilience-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-funds-resilience-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-funds-resilience-tests COMMAND tdx-funds-resilience-tests)

add_executable(tdx-institution-resilience-tests tests/institution_resilience_tests.cpp)
target_link_libraries(tdx-institution-resilience-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-institution-resilience-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-institution-resilience-tests COMMAND tdx-institution-resilience-tests)

add_executable(tdx-cloud-resilience-tests tests/cloud_resilience_tests.cpp)
target_link_libraries(tdx-cloud-resilience-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-cloud-resilience-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-cloud-resilience-tests COMMAND tdx-cloud-resilience-tests)

add_executable(tdx-jsn-retry-tests tests/jsn_retry_tests.cpp)
target_link_libraries(tdx-jsn-retry-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-jsn-retry-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-jsn-retry-tests COMMAND tdx-jsn-retry-tests)

add_executable(tdx-transport-retry-tests tests/transport_retry_tests.cpp)
target_link_libraries(tdx-transport-retry-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-transport-retry-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-transport-retry-tests COMMAND tdx-transport-retry-tests)

add_executable(tdx-jsn-variants-tests tests/jsn_variants_tests.cpp)
target_link_libraries(tdx-jsn-variants-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-jsn-variants-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-jsn-variants-tests COMMAND tdx-jsn-variants-tests)

add_executable(tdx-panorama-tests tests/panorama_tests.cpp)
target_link_libraries(tdx-panorama-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-panorama-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-panorama-tests COMMAND tdx-panorama-tests)

add_executable(tdx-institution-analysis-tests tests/institution_analysis_tests.cpp)
target_link_libraries(tdx-institution-analysis-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-institution-analysis-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-institution-analysis-tests COMMAND tdx-institution-analysis-tests)

add_executable(tdx-relative-valuation-tests tests/relative_valuation_tests.cpp)
target_link_libraries(tdx-relative-valuation-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-relative-valuation-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-relative-valuation-tests COMMAND tdx-relative-valuation-tests)

add_executable(tdx-factors-tests tests/factors_tests.cpp)
target_link_libraries(tdx-factors-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-factors-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-factors-tests COMMAND tdx-factors-tests)

add_executable(tdx-formula-calc-tests tests/formula_calc_tests.cpp)
target_link_libraries(tdx-formula-calc-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-formula-calc-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-calc-tests COMMAND tdx-formula-calc-tests)

add_executable(tdx-formula-bundle-tests tests/formula_bundle_tests.cpp)
target_link_libraries(tdx-formula-bundle-tests PRIVATE tdx_native)
add_dependencies(tdx-formula-bundle-tests tdx-formula-assets)
if(MINGW)
    target_link_options(tdx-formula-bundle-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-bundle-tests COMMAND tdx-formula-bundle-tests)

add_executable(tdx-cloud-calc-tests tests/cloud_calc_tests.cpp)
target_link_libraries(tdx-cloud-calc-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-cloud-calc-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-cloud-calc-tests COMMAND tdx-cloud-calc-tests)

add_executable(tdx-formula-engine-tests
    tests/formula_engine_tests.cpp
    tests/formula_engine_test_support.cpp
    tests/formula_engine_context_builder_tests.cpp
    tests/formula_engine_language_tests.cpp
    tests/formula_engine_native_operator_tests.cpp
    tests/formula_engine_native_binary_operator_tests.cpp
    tests/formula_engine_native_scalar_tests.cpp
    tests/formula_engine_workflow_tests.cpp
    tests/formula_engine_render_tests.cpp
    tests/formula_engine_context_library_tests.cpp
)
target_link_libraries(tdx-formula-engine-tests PRIVATE tdx_native)
target_compile_definitions(tdx-formula-engine-tests PRIVATE
    TDX_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
if(MINGW)
    target_link_options(tdx-formula-engine-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-engine-tests COMMAND tdx-formula-engine-tests)

add_executable(tdx-formula-context-aggregate-tests
    tests/formula_context_aggregate_tests.cpp
    tests/formula_engine_test_support.cpp
    tests/formula_engine_context_builder_tests.cpp
)
target_link_libraries(tdx-formula-context-aggregate-tests PRIVATE tdx_native)
target_compile_definitions(tdx-formula-context-aggregate-tests PRIVATE
    TDX_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
if(MINGW)
    target_link_options(tdx-formula-context-aggregate-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-context-aggregate-tests
    COMMAND tdx-formula-context-aggregate-tests)

add_executable(tdx-formula-context-capture-tests
    tests/formula_context_capture_tests.cpp)
target_link_libraries(tdx-formula-context-capture-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-formula-context-capture-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-context-capture-tests
    COMMAND tdx-formula-context-capture-tests)

add_executable(tdx-formula-future-replay-tests
    tests/formula_future_replay_tests.cpp)
target_link_libraries(tdx-formula-future-replay-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-formula-future-replay-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-future-replay-tests
    COMMAND tdx-formula-future-replay-tests)

add_executable(tdx-server-formula-context-import-tests
    tests/server_formula_context_import_tests.cpp)
target_link_libraries(tdx-server-formula-context-import-tests PRIVATE tdx_native)
target_include_directories(tdx-server-formula-context-import-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-formula-context-import-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-formula-context-import-tests
    COMMAND tdx-server-formula-context-import-tests)

add_executable(tdx-formula-strategy-tests tests/formula_strategy_tests.cpp)
target_link_libraries(tdx-formula-strategy-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-formula-strategy-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-formula-strategy-tests COMMAND tdx-formula-strategy-tests)

if(TDX_BUILD_TCALC_EXTRACTOR)
    add_executable(tdx-formulas-tests tests/formulas_tests.cpp)
    target_link_libraries(tdx-formulas-tests PRIVATE tdx_formula_extractor)
    target_compile_definitions(tdx-formulas-tests PRIVATE
        TDX_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_dependencies(tdx-formulas-tests tdx-formula-assets)
    if(MINGW)
        target_link_options(tdx-formulas-tests PRIVATE
            -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME tdx-formulas-tests COMMAND tdx-formulas-tests)
endif()

add_executable(tdx-tpool-tests tests/tpool_tests.cpp)
target_link_libraries(tdx-tpool-tests PRIVATE tdx_native)
target_include_directories(tdx-tpool-tests PRIVATE src/cloud)
target_compile_definitions(tdx-tpool-tests PRIVATE
    TDX_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
if(MINGW)
    target_link_options(tdx-tpool-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-tpool-tests COMMAND tdx-tpool-tests)

add_executable(tdx-level2-tests tests/level2_tests.cpp)
target_link_libraries(tdx-level2-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-level2-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-tests COMMAND tdx-level2-tests)

add_executable(tdx-level2-tpbus-115-batch-tests
    tests/level2_tpbus_115_batch_tests.cpp)
target_link_libraries(tdx-level2-tpbus-115-batch-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-level2-tpbus-115-batch-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-tpbus-115-batch-tests
         COMMAND tdx-level2-tpbus-115-batch-tests)

add_executable(tdx-level2-sdk-json-4653-tests
    tests/level2_sdk_json_4653_tests.cpp)
target_link_libraries(tdx-level2-sdk-json-4653-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-json-4653-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-json-4653-tests
         COMMAND tdx-level2-sdk-json-4653-tests)

add_executable(tdx-level2-sdk-compatibility-preflight-tests
    tests/level2_sdk_compatibility_preflight_tests.cpp)
target_link_libraries(tdx-level2-sdk-compatibility-preflight-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-compatibility-preflight-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-compatibility-preflight-tests
         COMMAND tdx-level2-sdk-compatibility-preflight-tests)

add_executable(tdx-level2-sdk-quote-transition-tests
    tests/level2_sdk_quote_transition_tests.cpp)
target_link_libraries(tdx-level2-sdk-quote-transition-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-quote-transition-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-quote-transition-tests
         COMMAND tdx-level2-sdk-quote-transition-tests)

add_executable(tdx-level2-sdk-correlation-transition-tests
    tests/level2_sdk_correlation_transition_tests.cpp)
target_link_libraries(tdx-level2-sdk-correlation-transition-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-correlation-transition-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-correlation-transition-tests
         COMMAND tdx-level2-sdk-correlation-transition-tests)

add_executable(tdx-level2-sdk-180x-host-projection-tests
    tests/level2_sdk_180x_host_projection_tests.cpp)
target_link_libraries(tdx-level2-sdk-180x-host-projection-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-180x-host-projection-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-180x-host-projection-tests
         COMMAND tdx-level2-sdk-180x-host-projection-tests)

add_executable(tdx-level2-sdk-1803-depth-record-projection-tests
    tests/level2_sdk_1803_depth_record_projection_tests.cpp)
target_link_libraries(tdx-level2-sdk-1803-depth-record-projection-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-1803-depth-record-projection-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-1803-depth-record-projection-tests
         COMMAND tdx-level2-sdk-1803-depth-record-projection-tests)

add_executable(tdx-level2-sdk-18031-queue-record-projection-tests
    tests/level2_sdk_18031_queue_record_projection_tests.cpp)
target_link_libraries(tdx-level2-sdk-18031-queue-record-projection-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-18031-queue-record-projection-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-18031-queue-record-projection-tests
         COMMAND tdx-level2-sdk-18031-queue-record-projection-tests)

add_executable(tdx-level2-sdk-1804-host-projection-tests
    tests/level2_sdk_1804_host_projection_tests.cpp)
target_link_libraries(tdx-level2-sdk-1804-host-projection-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-1804-host-projection-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-1804-host-projection-tests
         COMMAND tdx-level2-sdk-1804-host-projection-tests)

add_executable(tdx-level2-sdk-fixed-snapshot-projection-tests
    tests/level2_sdk_fixed_snapshot_projection_tests.cpp)
target_link_libraries(tdx-level2-sdk-fixed-snapshot-projection-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-fixed-snapshot-projection-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-fixed-snapshot-projection-tests
         COMMAND tdx-level2-sdk-fixed-snapshot-projection-tests)

add_executable(tdx-level2-sdk-4654-dual-snapshot-transition-tests
    tests/level2_sdk_4654_dual_snapshot_transition_tests.cpp)
target_link_libraries(tdx-level2-sdk-4654-dual-snapshot-transition-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-4654-dual-snapshot-transition-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-4654-dual-snapshot-transition-tests
         COMMAND tdx-level2-sdk-4654-dual-snapshot-transition-tests)

add_executable(tdx-level2-sdk-4651-dual-snapshot-transition-tests
    tests/level2_sdk_4651_dual_snapshot_transition_tests.cpp)
target_link_libraries(tdx-level2-sdk-4651-dual-snapshot-transition-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-4651-dual-snapshot-transition-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-4651-dual-snapshot-transition-tests
         COMMAND tdx-level2-sdk-4651-dual-snapshot-transition-tests)

add_executable(tdx-level2-sdk-4655-companion-raw-transition-tests
    tests/level2_sdk_4655_companion_raw_transition_tests.cpp)
target_link_libraries(tdx-level2-sdk-4655-companion-raw-transition-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-sdk-4655-companion-raw-transition-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-sdk-4655-companion-raw-transition-tests
         COMMAND tdx-level2-sdk-4655-companion-raw-transition-tests)

add_executable(tdx-level2-tpbus-4650-dispatch-preflight-tests
    tests/level2_tpbus_4650_dispatch_preflight_tests.cpp)
target_link_libraries(tdx-level2-tpbus-4650-dispatch-preflight-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-tpbus-4650-dispatch-preflight-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-tpbus-4650-dispatch-preflight-tests
         COMMAND tdx-level2-tpbus-4650-dispatch-preflight-tests)

add_executable(tdx-level2-tpbus-4650-price-primitives-tests
    tests/level2_tpbus_4650_price_primitives_tests.cpp)
target_link_libraries(tdx-level2-tpbus-4650-price-primitives-tests PRIVATE
    tdx_native)
if(MINGW)
    target_link_options(tdx-level2-tpbus-4650-price-primitives-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-tpbus-4650-price-primitives-tests
         COMMAND tdx-level2-tpbus-4650-price-primitives-tests)

add_executable(tdx-level2-formula-context-tests
    tests/level2_formula_context_tests.cpp)
target_link_libraries(tdx-level2-formula-context-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-level2-formula-context-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-level2-formula-context-tests
         COMMAND tdx-level2-formula-context-tests)

add_executable(tdx-server-level2-tests tests/server_level2_tests.cpp)
target_link_libraries(tdx-server-level2-tests PRIVATE tdx_native)
target_include_directories(tdx-server-level2-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-level2-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-level2-tests COMMAND tdx-server-level2-tests)

add_executable(tdx-server-level2-project-tests
    tests/server_level2_project_tests.cpp)
target_link_libraries(tdx-server-level2-project-tests PRIVATE tdx_native)
target_include_directories(tdx-server-level2-project-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-level2-project-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-level2-project-tests
         COMMAND tdx-server-level2-project-tests)

add_executable(tdx-server-catalog-tests tests/server_catalog_tests.cpp)
target_link_libraries(tdx-server-catalog-tests PRIVATE tdx_native)
target_include_directories(tdx-server-catalog-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-catalog-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-catalog-tests COMMAND tdx-server-catalog-tests)

add_executable(tdx-server-tqlex-tests tests/server_tqlex_tests.cpp)
target_link_libraries(tdx-server-tqlex-tests PRIVATE tdx_native)
target_include_directories(tdx-server-tqlex-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-tqlex-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-tqlex-tests COMMAND tdx-server-tqlex-tests)

add_executable(tdx-server-disclosure-tests
    tests/server_disclosure_tests.cpp)
target_link_libraries(tdx-server-disclosure-tests PRIVATE tdx_native)
target_include_directories(tdx-server-disclosure-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-disclosure-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-disclosure-tests
         COMMAND tdx-server-disclosure-tests)

add_executable(tdx-server-tpool-history-tests
    tests/server_tpool_history_tests.cpp)
target_link_libraries(tdx-server-tpool-history-tests PRIVATE tdx_native)
target_include_directories(tdx-server-tpool-history-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-tpool-history-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-tpool-history-tests
         COMMAND tdx-server-tpool-history-tests)

add_executable(tdx-server-formula-library-tests
    tests/server_formula_library_tests.cpp)
target_link_libraries(tdx-server-formula-library-tests PRIVATE tdx_native)
target_include_directories(tdx-server-formula-library-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-formula-library-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-formula-library-tests
         COMMAND tdx-server-formula-library-tests)

add_executable(tdx-server-option-surface-tests
    tests/server_option_surface_tests.cpp)
target_link_libraries(tdx-server-option-surface-tests PRIVATE tdx_native)
target_include_directories(tdx-server-option-surface-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-option-surface-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-option-surface-tests
         COMMAND tdx-server-option-surface-tests)

add_executable(tdx-server-local-resource-surface-tests
    tests/server_local_resource_surface_tests.cpp)
target_link_libraries(tdx-server-local-resource-surface-tests PRIVATE tdx_native)
target_include_directories(tdx-server-local-resource-surface-tests PRIVATE src/server)
if(MINGW)
    target_link_options(tdx-server-local-resource-surface-tests PRIVATE
        -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-server-local-resource-surface-tests
         COMMAND tdx-server-local-resource-surface-tests)

add_executable(tdx-futures-issuance-tests tests/futures_issuance_tests.cpp)
target_link_libraries(tdx-futures-issuance-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-futures-issuance-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-futures-issuance-tests COMMAND tdx-futures-issuance-tests)

add_executable(tdx-employees-tests tests/employees_tests.cpp)
target_link_libraries(tdx-employees-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-employees-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-employees-tests COMMAND tdx-employees-tests)

add_executable(tdx-hk-events-tests tests/hk_events_tests.cpp)
target_link_libraries(tdx-hk-events-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-hk-events-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-hk-events-tests COMMAND tdx-hk-events-tests)

add_executable(tdx-hk-actions-tests tests/hk_actions_tests.cpp)
target_link_libraries(tdx-hk-actions-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-hk-actions-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-hk-actions-tests COMMAND tdx-hk-actions-tests)

add_executable(tdx-hk-finance-tests tests/hk_finance_tests.cpp)
target_link_libraries(tdx-hk-finance-tests PRIVATE tdx_native)
target_include_directories(tdx-hk-finance-tests PRIVATE src/industry src/formula)
if(MINGW)
    target_link_options(tdx-hk-finance-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-hk-finance-tests COMMAND tdx-hk-finance-tests)

add_executable(tdx-investment-tests tests/investment_tests.cpp)
target_link_libraries(tdx-investment-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-investment-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-investment-tests COMMAND tdx-investment-tests)

add_executable(tdx-shape-match-tests tests/shape_match_tests.cpp)
target_link_libraries(tdx-shape-match-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-shape-match-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-shape-match-tests COMMAND tdx-shape-match-tests)

add_executable(tdx-special-situations-tests tests/special_situations_tests.cpp)
target_link_libraries(tdx-special-situations-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-special-situations-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-special-situations-tests COMMAND tdx-special-situations-tests)

add_executable(tdx-exchange-funds-tests tests/exchange_funds_tests.cpp)
target_link_libraries(tdx-exchange-funds-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-exchange-funds-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-exchange-funds-tests COMMAND tdx-exchange-funds-tests)

add_executable(tdx-fund-reference-tests tests/fund_reference_tests.cpp)
target_link_libraries(tdx-fund-reference-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-fund-reference-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-fund-reference-tests COMMAND tdx-fund-reference-tests)

add_executable(tdx-curated-data-tests tests/curated_data_tests.cpp)
target_link_libraries(tdx-curated-data-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-curated-data-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-curated-data-tests COMMAND tdx-curated-data-tests)

add_executable(tdx-special-attention-tests tests/special_attention_tests.cpp)
target_link_libraries(tdx-special-attention-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-special-attention-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-special-attention-tests COMMAND tdx-special-attention-tests)

add_executable(tdx-specialized-metrics-tests tests/specialized_metrics_tests.cpp)
target_link_libraries(tdx-specialized-metrics-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-specialized-metrics-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-specialized-metrics-tests COMMAND tdx-specialized-metrics-tests)

add_executable(tdx-company-changes-tests tests/company_changes_tests.cpp)
target_link_libraries(tdx-company-changes-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-company-changes-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-company-changes-tests COMMAND tdx-company-changes-tests)

add_executable(tdx-financial-screen-tests tests/financial_screen_tests.cpp)
target_link_libraries(tdx-financial-screen-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-financial-screen-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-financial-screen-tests COMMAND tdx-financial-screen-tests)

add_executable(tdx-equity-performance-tests tests/equity_performance_tests.cpp)
target_link_libraries(tdx-equity-performance-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-equity-performance-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-equity-performance-tests COMMAND tdx-equity-performance-tests)

add_executable(tdx-shareholder-signals-tests tests/shareholder_signals_tests.cpp)
target_link_libraries(tdx-shareholder-signals-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-shareholder-signals-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-shareholder-signals-tests COMMAND tdx-shareholder-signals-tests)

add_executable(tdx-financial-insights-tests tests/financial_insights_tests.cpp)
target_link_libraries(tdx-financial-insights-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-financial-insights-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-financial-insights-tests COMMAND tdx-financial-insights-tests)

add_executable(tdx-gdr-tests tests/gdr_tests.cpp)
target_link_libraries(tdx-gdr-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-gdr-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-gdr-tests COMMAND tdx-gdr-tests)

add_executable(tdx-fund-calendar-tests tests/fund_calendar_tests.cpp)
target_link_libraries(tdx-fund-calendar-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-fund-calendar-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-fund-calendar-tests COMMAND tdx-fund-calendar-tests)

add_executable(tdx-corporate-orders-tests tests/corporate_orders_tests.cpp)
target_link_libraries(tdx-corporate-orders-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-corporate-orders-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-corporate-orders-tests COMMAND tdx-corporate-orders-tests)

add_executable(tdx-event-impact-tests tests/event_impact_tests.cpp)
target_link_libraries(tdx-event-impact-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-event-impact-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-event-impact-tests COMMAND tdx-event-impact-tests)

add_executable(tdx-global-performance-tests tests/global_performance_tests.cpp)
target_link_libraries(tdx-global-performance-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-global-performance-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-global-performance-tests COMMAND tdx-global-performance-tests)

add_executable(tdx-recent-watch-tests tests/recent_watch_tests.cpp)
target_link_libraries(tdx-recent-watch-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-recent-watch-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-recent-watch-tests COMMAND tdx-recent-watch-tests)

add_executable(tdx-patent-statistics-tests tests/patent_statistics_tests.cpp)
target_link_libraries(tdx-patent-statistics-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-patent-statistics-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-patent-statistics-tests COMMAND tdx-patent-statistics-tests)

add_executable(tdx-unlock-monthly-tests tests/unlock_monthly_tests.cpp)
target_link_libraries(tdx-unlock-monthly-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-unlock-monthly-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-unlock-monthly-tests COMMAND tdx-unlock-monthly-tests)

add_executable(tdx-overview-factors-tests tests/overview_factors_tests.cpp)
target_link_libraries(tdx-overview-factors-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-overview-factors-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-overview-factors-tests COMMAND tdx-overview-factors-tests)

add_executable(tdx-benchmark-analysis-tests tests/benchmark_analysis_tests.cpp)
target_link_libraries(tdx-benchmark-analysis-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-benchmark-analysis-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-benchmark-analysis-tests COMMAND tdx-benchmark-analysis-tests)

add_executable(tdx-fund-statistics-tests tests/fund_statistics_tests.cpp)
target_link_libraries(tdx-fund-statistics-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-fund-statistics-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-fund-statistics-tests COMMAND tdx-fund-statistics-tests)

add_executable(tdx-calendar-tests tests/calendar_tests.cpp)
target_link_libraries(tdx-calendar-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-calendar-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-calendar-tests COMMAND tdx-calendar-tests)

add_executable(tdx-roadshows-tests tests/roadshows_tests.cpp)
target_link_libraries(tdx-roadshows-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-roadshows-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-roadshows-tests COMMAND tdx-roadshows-tests)

add_executable(tdx-cloud-variants-tests tests/cloud_variants_tests.cpp)
target_link_libraries(tdx-cloud-variants-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-cloud-variants-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-cloud-variants-tests COMMAND tdx-cloud-variants-tests)

add_executable(tdx-market-stream-tests tests/market_stream_tests.cpp)
target_link_libraries(tdx-market-stream-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-market-stream-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-market-stream-tests COMMAND tdx-market-stream-tests)

add_executable(tdx-image-data-tests tests/image_data_tests.cpp)
target_link_libraries(tdx-image-data-tests PRIVATE tdx_native)
if(MINGW)
    target_link_options(tdx-image-data-tests PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME tdx-image-data-tests COMMAND tdx-image-data-tests)

install(TARGETS tdx-tool RUNTIME DESTINATION bin)
if(TDX_BUILD_TCALC_EXTRACTOR)
    install(TARGETS tdx-formula-extractor RUNTIME DESTINATION bin)
endif()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../web/dist/
        DESTINATION share/tdx-tool/web)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/formula/
        DESTINATION share/tdx-tool/formula-assets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/recon/
        DESTINATION share/tdx-tool/recon-assets)
