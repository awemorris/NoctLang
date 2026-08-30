/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Target-neutral DOALL classification.
 */

#include "hir_opt_paralle.h"

#include <stdlib.h>
#include <string.h>

/* Forward declarations. */
static const struct hir_memory_object *hir_doall_object(const struct hir_loop_summary *summary, int id);
static bool hir_doall_disjoint(const struct hir_memory_object *first, const struct hir_memory_object *second);
static int hir_doall_dep_kind(const struct hir_memory_access *first, const struct hir_memory_access *second, int *distance);
static int hir_doall_reason_for_kind(int kind);
static bool hir_doall_record_alias_requirement(struct hir_doall_result *result, int first_object_id, int second_object_id);
static bool hir_doall_record_dependence(struct hir_doall_result *result, uint32_t first, uint32_t second, int kind, bool distance_known, int distance);
static bool hir_doall_classify_internal(const struct hir_loop_summary *summary_const, struct hir_doall_result *result, bool check_scalars);

bool
hir_doall_classify(
	const struct hir_loop_summary *summary,
	struct hir_doall_result *result)
{
	return hir_doall_classify_internal(summary, result, true);
}

bool
hir_doall_classify_memory(
	const struct hir_loop_summary *summary,
	struct hir_doall_result *result)
{
	return hir_doall_classify_internal(summary, result, false);
}

static const struct hir_memory_object *
hir_doall_object(
	const struct hir_loop_summary *summary,
	int id)
{
	uint32_t i;

	if (summary->catalog == NULL)
		return NULL;
	for (i = 0; i < summary->catalog->count; i++) {
		if (summary->catalog->object[i].id == id)
			return &summary->catalog->object[i];
	}
	return NULL;
}

static bool
hir_doall_disjoint(
	const struct hir_memory_object *first,
	const struct hir_memory_object *second)
{
	if (first->id == second->id)
		return false;
	if (first->alias_kind == HIR_ALIAS_UNIQUE ||
	    second->alias_kind == HIR_ALIAS_UNIQUE)
		return true;
	if (first->alias_kind == HIR_ALIAS_CHECKED_NOALIAS &&
	    second->alias_kind == HIR_ALIAS_CHECKED_NOALIAS)
		return true;
	return false;
}

static int
hir_doall_dep_kind(
	const struct hir_memory_access *first,
	const struct hir_memory_access *second,
	int *distance)
{
	const struct hir_memory_access *write;
	const struct hir_memory_access *read;
	int delta;

	if (first->kind == HIR_MEMORY_WRITE &&
	    second->kind == HIR_MEMORY_WRITE) {
		*distance = first->index.offset - second->index.offset;
		return HIR_DEP_WAW;
	}
	write = first->kind == HIR_MEMORY_WRITE ? first : second;
	read = first->kind == HIR_MEMORY_READ ? first : second;
	delta = write->index.offset - read->index.offset;
	if (delta > 0) {
		*distance = delta;
		return HIR_DEP_RAW;
	}
	*distance = delta < 0 ? -delta : 0;
	return HIR_DEP_WAR;
}

static int
hir_doall_reason_for_kind(
	int kind)
{
	if (kind == HIR_DEP_RAW)
		return HIR_PAR_REASON_MEMORY_RAW;
	if (kind == HIR_DEP_WAR)
		return HIR_PAR_REASON_MEMORY_WAR;
	return HIR_PAR_REASON_MEMORY_WAW;
}

static bool
hir_doall_record_alias_requirement(
	struct hir_doall_result *result,
	int first_object_id,
	int second_object_id)
{
	uint32_t i;
	int tmp;

	if (first_object_id > second_object_id) {
		tmp = first_object_id;
		first_object_id = second_object_id;
		second_object_id = tmp;
	}
	for (i = 0; i < result->alias_requirement_count; i++) {
		if (result->alias_requirement[i].first_object_id == first_object_id &&
		    result->alias_requirement[i].second_object_id == second_object_id)
			return true;
	}
	if (result->alias_requirement_count >= HIR_PARALLEL_MAX_DEPENDENCES)
		return false;
	result->alias_requirement[result->alias_requirement_count].first_object_id =
		first_object_id;
	result->alias_requirement[result->alias_requirement_count].second_object_id =
		second_object_id;
	result->alias_requirement_count++;
	return true;
}

static bool
hir_doall_record_dependence(
	struct hir_doall_result *result,
	uint32_t first,
	uint32_t second,
	int kind,
	bool distance_known,
	int distance)
{
	struct hir_dependence *dependence;

	if (result->dependence_count >= HIR_PARALLEL_MAX_DEPENDENCES)
		return false;
	dependence = &result->dependence[result->dependence_count++];
	memset(dependence, 0, sizeof(*dependence));
	dependence->kind = kind;
	dependence->first_access = first;
	dependence->second_access = second;
	dependence->distance_known = distance_known;
	dependence->distance = distance;
	return true;
}

static bool
hir_doall_classify_internal(
	const struct hir_loop_summary *summary_const,
	struct hir_doall_result *result,
	bool check_scalars)
{
	const struct hir_scalar_effect *scalar;
	const struct hir_memory_access *first;
	const struct hir_memory_access *second;
	const struct hir_memory_object *first_object;
	const struct hir_memory_object *second_object;
	uint32_t i;
	uint32_t j;
	int kind;
	int distance;
	int reason;
	bool dependent;

	if (summary_const == NULL || result == NULL)
		return false;
	memset(result, 0, sizeof(*result));
	result->classification = HIR_PAR_CLASS_UNKNOWN;
	result->reason = HIR_PAR_REASON_NONE;
	if (summary_const->analysis_status != HIR_ANALYSIS_COMPLETE) {
		result->reason = summary_const->analysis_reason;
		return true;
	}
	for (i = 0; check_scalars && i < summary_const->scalar_count; i++) {
		scalar = &summary_const->scalar[i];
		if (scalar->is_counter || scalar->writes == 0)
			continue;
		if (scalar->declared_inside_loop) {
			if (!scalar->read_before_write)
				continue;
			result->classification = HIR_PAR_CLASS_DEPENDENT;
			result->reason = HIR_PAR_REASON_SCALAR_CARRIED;
			return true;
		}
		result->classification = HIR_PAR_CLASS_DEPENDENT;
		result->reason = scalar->reads != 0 ?
			HIR_PAR_REASON_SCALAR_CARRIED :
			HIR_PAR_REASON_OUTER_SCALAR_WRITE;
		return true;
	}
	dependent = false;
	reason = HIR_PAR_REASON_NONE;
	for (i = 0; i < summary_const->access_count; i++) {
		first = &summary_const->access[i];
		if (first->kind == HIR_MEMORY_WRITE &&
		    first->index.kind == HIR_AFFINE_INVARIANT) {
			if (!hir_doall_record_dependence(result, i, i,
						  HIR_DEP_WAW, false, 0)) {
				result->reason = HIR_PAR_REASON_DEPENDENCE_LIMIT;
				return true;
			}
			dependent = true;
			if (reason == HIR_PAR_REASON_NONE)
				reason = HIR_PAR_REASON_MEMORY_WAW;
		}
		for (j = i + 1; j < summary_const->access_count; j++) {
			second = &summary_const->access[j];
			if (first->kind != HIR_MEMORY_WRITE &&
			    second->kind != HIR_MEMORY_WRITE)
				continue;
			first_object = hir_doall_object(summary_const,
						       first->object_id);
			second_object = hir_doall_object(summary_const,
							second->object_id);
			if (first_object == NULL || second_object == NULL) {
				result->reason = HIR_PAR_REASON_UNKNOWN_MEMORY;
				return true;
			}
			if (hir_doall_disjoint(first_object, second_object))
				continue;
			if (first_object->id != second_object->id) {
				if (!hir_doall_record_alias_requirement(result,
							 first_object->id,
							 second_object->id)) {
					result->reason = HIR_PAR_REASON_DEPENDENCE_LIMIT;
					return true;
				}
				continue;
			}
			if (first->index.kind == HIR_AFFINE_COUNTER_OFFSET &&
			    second->index.kind == HIR_AFFINE_COUNTER_OFFSET) {
				if (hir_parallel_affine_equal(&first->index,
							      &second->index))
					continue;
				if (first->index.invariant_symbol == NULL &&
				    second->index.invariant_symbol == NULL)
					kind = hir_doall_dep_kind(first, second, &distance);
				else {
					kind = first->kind == HIR_MEMORY_WRITE &&
					       second->kind == HIR_MEMORY_WRITE ?
						HIR_DEP_WAW :
						(first->kind == HIR_MEMORY_WRITE ?
						 HIR_DEP_RAW : HIR_DEP_WAR);
					distance = 0;
				}
				if (!hir_doall_record_dependence(result, i, j,
							  kind,
							  first->index.invariant_symbol == NULL &&
							  second->index.invariant_symbol == NULL,
							  distance)) {
					result->reason =
						HIR_PAR_REASON_DEPENDENCE_LIMIT;
					return true;
				}
				dependent = true;
				if (reason == HIR_PAR_REASON_NONE)
					reason = hir_doall_reason_for_kind(kind);
				continue;
			}
			kind = first->kind == HIR_MEMORY_WRITE &&
			       second->kind == HIR_MEMORY_WRITE ?
				HIR_DEP_WAW :
				(first->kind == HIR_MEMORY_WRITE ?
				 HIR_DEP_RAW : HIR_DEP_WAR);
			if (!hir_doall_record_dependence(result, i, j,
						  kind, false, 0)) {
				result->reason = HIR_PAR_REASON_DEPENDENCE_LIMIT;
				return true;
			}
			dependent = true;
			if (reason == HIR_PAR_REASON_NONE)
				reason = hir_doall_reason_for_kind(kind);
		}
	}
	if (dependent) {
		result->classification = HIR_PAR_CLASS_DEPENDENT;
		result->reason = reason;
	} else if (result->alias_requirement_count != 0) {
		result->classification = HIR_PAR_CLASS_UNKNOWN;
		result->reason = HIR_PAR_REASON_MAY_ALIAS;
	} else {
		result->classification = HIR_PAR_CLASS_DOALL;
		result->reason = HIR_PAR_REASON_NONE;
	}
	return true;
}
