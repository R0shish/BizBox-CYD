#pragma once
// ==========================================================================
//  Connect presets: one click fills the Connect tab's paths, status map and
//  events for a known system. Format: docs/CONNECT.md, section 4.
//  Never put URLs, tokens or secrets in a preset: this file is public.
// ==========================================================================

static const char CONNECT_PRESETS[] PROGMEM = R"JSON([
{"name": "Generic (BizBox defaults)",
 "pull_orders": "orders", "pull_id": "id", "pull_title": "title", "pull_amount": "amount",
 "pull_status": "status", "pull_commands": "commands", "pull_cursor": "cursor",
 "status_map": "NEW=new,pending,received,placed; PREP=preparing,processing,in_progress,printing; READY=ready,ready_for_pickup,done_printing; PAID=paid,completed,delivered,picked_up; REMOVE=cancelled,canceled,deleted,archived",
 "events": 7},
{"name": "Webhooks only (payments to a sheet or chat)",
 "pull_orders": "orders", "pull_id": "id", "pull_title": "title", "pull_amount": "amount",
 "pull_status": "status", "pull_commands": "commands", "pull_cursor": "cursor",
 "status_map": "", "events": 1}
// ======================= VENDOR PRESETS ==================================
// Add your system below as one more object (mind the comma before it), e.g.
// ,{"name": "My POS", "pull_orders": "data.orders", "pull_id": "uuid",
//   "pull_title": "summary", "pull_amount": "total", "pull_status": "state",
//   "pull_commands": "data.commands", "pull_cursor": "next",
//   "status_map": "NEW=open; PREP=cooking; READY=ready; PAID=closed; REMOVE=void",
//   "events": 7}
// =========================================================================
])JSON";
