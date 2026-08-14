/**
 * Short-lived handoff from the Level2 decoder to the formula workbench.
 * The in-memory copy handles large decoded documents; sessionStorage keeps a
 * normal-sized handoff alive across a reload without creating a database/file.
 */

const STORAGE_KEY = 'tdx.level2.formula-context.v1';
const ALLOWED_SCHEMAS = new Set([
  'tdx-level2-tcalc-order-flow-v1',
  'tdx-level2-tcalc-order-side-v1'
]);

let stagedDocument = '';

export interface StagedFormulaContext {
  schema: string;
  document: Record<string, unknown>;
}

function decode(value: string): StagedFormulaContext | null {
  if (!value) return null;
  try {
    const document: unknown = JSON.parse(value);
    if (!document || typeof document !== 'object' || Array.isArray(document)) return null;
    const schema = (document as Record<string, unknown>).schema;
    if (typeof schema !== 'string' || !ALLOWED_SCHEMAS.has(schema)) return null;
    return { schema, document: document as Record<string, unknown> };
  } catch {
    return null;
  }
}

export function stageLevel2FormulaContext(document: Record<string, unknown>): void {
  const schema = document.schema;
  if (typeof schema !== 'string' || !ALLOWED_SCHEMAS.has(schema))
    throw new Error('当前 Level2 解码结果不能作为公式上下文');
  stagedDocument = JSON.stringify(document);
  try {
    sessionStorage.setItem(STORAGE_KEY, stagedDocument);
  } catch {
    // The module-level copy remains available when the decoded JSON exceeds
    // the browser's sessionStorage quota.
  }
}

export function takeStagedLevel2FormulaContext(): StagedFormulaContext | null {
  let value = stagedDocument;
  if (!value) {
    try {
      value = sessionStorage.getItem(STORAGE_KEY) ?? '';
    } catch {
      value = '';
    }
  }
  stagedDocument = '';
  try {
    sessionStorage.removeItem(STORAGE_KEY);
  } catch {
    // A disabled storage backend does not affect the in-memory handoff.
  }
  return decode(value);
}
