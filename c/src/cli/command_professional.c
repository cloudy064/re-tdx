/* Professional-data loading, selection and output orchestration. */
#include "cli_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tdx_professional.h"
#include "tdx_professional_finance.h"
#include "tdx_professional_finance_json.h"
#include "tdx_professional_json.h"
#include "tdx_zip.h"

static int command_professional_finance(const cli_professional_options *options, tdx_error *err) {
    tdx_buf archive = {0}, member = {0}, line = {0};
    tdx_zip_entry entries[TDX_ZIP_ENTRIES_MAX], chosen;
    tdx_profinance_document document;
    FILE *input = NULL, *stream = NULL;
    const char *member_name;
    long length;
    size_t entry_count = 0, emitted = 0, index;
    int status = TDX_ERR;

    input = fopen(options->zip, "rb");
    if (!input) {
        tdx_error_set(err, "cannot open %s", options->zip);
        goto done;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size %s", options->zip);
        goto done;
    }
    if (length > 0) {
        if (tdx_buf_reserve(&archive, (size_t)length, err) != TDX_OK)
            goto done;
        if (fread(archive.data, 1, (size_t)length, input) != (size_t)length) {
            tdx_error_set(err, "cannot read %s", options->zip);
            goto done;
        }
        archive.len = (size_t)length;
    }
    fclose(input);
    input = NULL;
    if (tdx_zip_entries(archive.data, archive.len, entries, TDX_ZIP_ENTRIES_MAX,
                        &entry_count, err) != TDX_OK)
        goto done;
    if (!entry_count) {
        tdx_error_set(err, "the ZIP archive has no members");
        goto done;
    }
    member_name = options->zip_member ? options->zip_member :
                  (options->kind ? options->kind : entries[0].name);
    if (!tdx_zip_find(entries, entry_count, member_name, &chosen)) {
        tdx_error_set(err, "%s holds no member named %s", options->zip, member_name);
        goto done;
    }
    if (tdx_zip_extract(archive.data, archive.len, &chosen, &member, err) != TDX_OK ||
        tdx_profinance_parse(member.data, member.len, &document, err) != TDX_OK)
        goto done;
    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    if (tdx_profinance_format_document(&line, options->zip, chosen.name,
                                       chosen.uncompressed_size, chosen.crc,
                                       &document, err) != TDX_OK ||
        cli_write_json_line(stream, &line, err) != TDX_OK)
        goto done;

    if (options->code && *options->code) {
        for (index = 0; index < document.record_count; ++index) {
            tdx_profinance_record_view view;
            unsigned field;
            if (tdx_profinance_record_at(&document, index, &view, err) != TDX_OK)
                goto done;
            if (strncmp(view.code, options->code, 6) != 0)
                continue;
            tdx_buf_clear(&line);
            if (tdx_profinance_format_record(&line, &view, document.report_date, err) != TDX_OK ||
                cli_write_json_line(stream, &line, err) != TDX_OK)
                goto done;
            for (field = 1; field <= view.field_count; ++field) {
                if (options->field_id && field != options->field_id)
                    continue;
                if (options->common.limit && emitted >= options->common.limit)
                    break;
                tdx_buf_clear(&line);
                if (tdx_profinance_format_field(&line, &view, field, err) != TDX_OK ||
                    cli_write_json_line(stream, &line, err) != TDX_OK)
                    goto done;
                emitted++;
            }
            break;
        }
        if (!emitted) {
            tdx_error_set(err, "%s holds no matching fields for code %s", options->zip, options->code);
            goto done;
        }
    } else {
        for (index = 0; index < document.record_count; ++index) {
            tdx_profinance_record_view view;
            if (options->common.limit && emitted >= options->common.limit)
                break;
            if (tdx_profinance_record_at(&document, index, &view, err) != TDX_OK)
                goto done;
            tdx_buf_clear(&line);
            if (tdx_profinance_format_row(&line, &view, document.report_date,
                                          options->field_id, err) != TDX_OK ||
                cli_write_json_line(stream, &line, err) != TDX_OK)
                goto done;
            emitted++;
        }
    }
    status = TDX_OK;
    if (!options->common.quiet)
        fprintf(stderr,
                "professional %s: member %s %u bytes crc %08x, version %u, report date %u, "
                "%zu records of %zu fields, %zu emitted\n",
                options->zip, chosen.name, chosen.uncompressed_size, chosen.crc,
                document.version, document.report_date, document.record_count,
                document.field_count, emitted);
done:
    if (input)
        fclose(input);
    status = cli_finish_output(stream, status, err);
    tdx_buf_free(&archive);
    tdx_buf_free(&member);
    tdx_buf_free(&line);
    return status;
}

int cli_command_professional(const cli_professional_options *options, tdx_error *err) {
    static tdx_buf raw = {0};
    static tdx_buf line = {0};
    tdx_professional_record *records = NULL;
    tdx_professional_id_summary *fields = NULL;
    size_t *indices = NULL;
    FILE *input = NULL;
    FILE *stream = NULL;
    long length;
    tdx_professional_kind kind = TDX_PROFESSIONAL_STOCK;
    tdx_professional_selection selection;
    size_t record_count = 0;
    size_t field_count = 0;
    size_t selected = 0;
    size_t named = 0;
    size_t unnamed = 0;
    size_t first_date = 0;
    size_t last_date = 0;
    size_t index;
    int status = TDX_ERR;

    if (options->zip && *options->zip)
        return command_professional_finance(options, err);
    if (!options->input || !*options->input) {
        tdx_error_set(err, "professional needs --input PATH or --zip PATH");
        return TDX_ERR;
    }
    if (options->kind && *options->kind &&
        !tdx_professional_kind_parse(options->kind, strlen(options->kind), &kind)) {
        tdx_error_set(err, "--kind must be stock, market or board");
        return TDX_ERR;
    }
    tdx_buf_init(&raw);
    tdx_buf_init(&line);
    input = fopen(options->input, "rb");
    if (!input) {
        tdx_error_set(err, "cannot open %s", options->input);
        goto done;
    }
    if (fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
        fseek(input, 0, SEEK_SET) != 0) {
        tdx_error_set(err, "cannot size %s", options->input);
        goto done;
    }
    if (length > 0) {
        if (tdx_buf_reserve(&raw, (size_t)length, err) != TDX_OK)
            goto done;
        if (fread(raw.data, 1, (size_t)length, input) != (size_t)length) {
            tdx_error_set(err, "cannot read %s", options->input);
            goto done;
        }
        raw.len = (size_t)length;
    }
    fclose(input);
    input = NULL;

    records = (tdx_professional_record *)calloc(TDX_PROFESSIONAL_RECORDS_MAX, sizeof(*records));
    fields = (tdx_professional_id_summary *)calloc(256, sizeof(*fields));
    indices = (size_t *)calloc(TDX_PROFESSIONAL_RECORDS_MAX, sizeof(*indices));
    if (!records || !fields || !indices) {
        tdx_error_set(err, "out of memory for the professional-data buffers");
        goto done;
    }
    if (tdx_professional_parse_trading(raw.data, raw.len, records, TDX_PROFESSIONAL_RECORDS_MAX,
                                       &record_count, err) != TDX_OK)
        goto done;
    if (tdx_professional_summarize(records, record_count, kind, fields, 256, &field_count,
                                   err) != TDX_OK)
        goto done;

    selection.id = options->field_id;
    selection.from = (uint32_t)(options->from_date > 0 ? options->from_date : 0);
    selection.to = (uint32_t)(options->to_date > 0 ? options->to_date : 0);
    if (tdx_professional_select(records, record_count, &selection, indices,
                                TDX_PROFESSIONAL_RECORDS_MAX, &selected, err) != TDX_OK)
        goto done;

    for (index = 0; index < field_count; ++index) {
        if (fields[index].name)
            named++;
        else
            unnamed++;
        if (fields[index].first_date) {
            if (first_date == 0 || fields[index].first_date < first_date)
                first_date = fields[index].first_date;
            if (fields[index].last_date > last_date)
                last_date = fields[index].last_date;
        }
    }

    stream = cli_open_output(&options->common);
    if (!stream) {
        tdx_error_set(err, "cannot open output %s",
                      options->common.output ? options->common.output : "<stdout>");
        goto done;
    }
    tdx_buf_clear(&line);
    if (tdx_professional_format_document(&line, options->input, kind, raw.len, record_count,
                                         field_count, named, unnamed, selected, first_date,
                                         last_date, err) != TDX_OK)
        goto done;
    if (cli_write_json_line(stream, &line, err) != TDX_OK)
        goto done;
    for (index = 0; index < field_count; ++index) {
        tdx_buf_clear(&line);
        if (tdx_professional_format_summary(&line, &fields[index], kind, err) != TDX_OK)
            goto done;
        if (cli_write_json_line(stream, &line, err) != TDX_OK)
            goto done;
    }
    for (index = 0; index < selected; ++index) {
        const tdx_professional_record *record = &records[indices[index]];
        tdx_buf_clear(&line);
        if (tdx_professional_format_record(&line, record,
                                           tdx_professional_field_name(kind, record->id),
                                           indices[index], err) != TDX_OK)
            goto done;
        if (cli_write_json_line(stream, &line, err) != TDX_OK)
            goto done;
    }
    status = TDX_OK;
    if (!options->common.quiet)
        fprintf(stderr,
                "professional %s: %zu bytes, %zu records, %zu fields (%zu named, %zu unnamed), "
                "%zu selected\n",
                options->input, raw.len, record_count, field_count, named, unnamed, selected);

done:
    if (input)
        fclose(input);
    status = cli_finish_output(stream, status, err);
    free(records);
    free(fields);
    free(indices);
    tdx_buf_free(&raw);
    tdx_buf_free(&line);
    return status;
}
