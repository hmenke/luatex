/* pdfshipout.c

   Copyright 2009 Taco Hoekwater <taco@luatex.org>

   This file is part of LuaTeX.

   LuaTeX is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   LuaTeX is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU General Public License along
   with LuaTeX; if not, see <http://www.gnu.org/licenses/>. */

static const char __svn_version[] =
    "$Id$"
    "$URL$";

#include "ptexlib.h"

#define count(A) eqtb[count_base+(A)].cint
#define h_offset dimen_par(h_offset_code)
#define mag int_par(mag_code)
#define page_bottom_offset dimen_par(page_bottom_offset_code)
#define page_direction int_par(page_direction_code)
#define page_height dimen_par(page_height_code)
#define page_left_offset dimen_par(page_left_offset_code)
#define page_right_offset dimen_par(page_right_offset_code)
#define page_top_offset dimen_par(page_top_offset_code)
#define page_width dimen_par(page_width_code)
#define pdf_h_origin dimen_par(pdf_h_origin_code)
#define pdf_v_origin dimen_par(pdf_v_origin_code)
#define tracing_output int_par(tracing_output_code)
#define tracing_stats int_par(tracing_stats_code)
#define v_offset dimen_par(v_offset_code)

scaledpos shipbox_refpos;

/*
|ship_out| is used to shipout a box to PDF or DVI mode.
If |shipping_page| is not set then the output will be a Form object
(only PDF), otherwise it will be a Page object.
*/

void ship_out(PDF pdf, halfword p, boolean shipping_page)
{
    /* output the box |p| */
    integer j, k;               /* DVI, PDF *//* indices to first ten count registers */
    integer post_callback_id;   /* DVI, PDF */
    integer pre_callback_id;    /* DVI, PDF */
    pdf_resource_struct resources;      /* PDF */
    posstructure refpoint;      /* DVI, PDF *//* the origin pos. on the page */
    scaledpos cur = { 0, 0 };   /* DVI, PDF */

    pdf->f_cur = null_font;

    switch (pdf->o_mode) {
    case OMODE_DVI:
        assert(shipping_page == true);
        init_dvi_output_functions(pdf);
        if (half_buf == 0) {
            half_buf = dvi_buf_size / 2;
            dvi_limit = dvi_buf_size;
        }
        break;
    case OMODE_PDF:
        check_pdfminorversion(pdf);     /* does also prepare_mag() */
        reset_resource_lists(&resources);
        pdf->resources = &resources;
        pdf->resources->last_resources = pdf_new_objnum(pdf);
        break;
    default:
        assert(0);
    }

    /* Start sheet {\sl Sync\TeX} information record */
    /* {\sl Sync\TeX}: we assume that |pdf_output| is properly set up */
    if (synctexoption == 1)
        synctex_sheet(mag);

    pre_callback_id = callback_defined(start_page_number_callback);
    post_callback_id = callback_defined(stop_page_number_callback);
    if ((tracing_output > 0) && (pre_callback_id == 0)) {
        tprint_nl("");
        print_ln();
        tprint("Completed box being shipped out");
    }
    is_shipping_page = shipping_page;
    if (shipping_page) {
        if (pre_callback_id > 0)
            (void) run_callback(pre_callback_id, "->");
        else if (pre_callback_id == 0) {
            if (term_offset > max_print_line - 9)
                print_ln();
            else if ((term_offset > 0) || (file_offset > 0))
                print_char(' ');
            print_char('[');
            j = 9;
            while ((count(j) == 0) && (j > 0))
                decr(j);
            for (k = 0; k <= j; k++) {
                print_int(count(k));
                if (k < j)
                    print_char('.');
            }
        }
    }
    if ((tracing_output > 0) && shipping_page) {
        print_char(']');
        update_terminal();
        begin_diagnostic();
        show_box(p);
        end_diagnostic(true);
    }

    /* Ship box |p| out */
    if (shipping_page && box_dir(p) != page_direction)
        pdf_warning("\\shipout",
                    "\\pagedir != \\bodydir; "
                    "\\box\\outputbox may be placed wrongly on the page.", true,
                    true);
    /* Update the values of |max_h| and |max_v|; but if the page is too large, |goto done| */
    /* Sometimes the user will generate a huge page because other error messages
       are being ignored. Such pages are not output to the \.{dvi} file, since they
       may confuse the printing software. */

    if ((height(p) > max_dimen) || (depth(p) > max_dimen)
        || (height(p) + depth(p) + v_offset > max_dimen)
        || (width(p) + h_offset > max_dimen)) {
        char *hlp[] = { "The page just created is more than 18 feet tall or",
            "more than 18 feet wide, so I suspect something went wrong.",
            NULL
        };
        tex_error("Huge page cannot be shipped out", hlp);
        if (tracing_output <= 0) {
            begin_diagnostic();
            tprint_nl("The following box has been deleted:");
            show_box(p);
            end_diagnostic(true);
        }
        goto DONE;
    }
    if (height(p) + depth(p) + v_offset > max_v)
        max_v = height(p) + depth(p) + v_offset;
    if (width(p) + h_offset > max_h)
        max_h = width(p) + h_offset;

    /* Calculate page dimensions and margins */
    if (is_shipping_page) {
        if (page_width > 0)
            cur_page_size.h = page_width;
        else {
            switch (box_direction(page_direction)) {
            case dir_TL_:
            case dir_BL_:
                cur_page_size.h = width(p) + 2 * page_left_offset;
                break;
            case dir_TR_:
            case dir_BR_:
                cur_page_size.h = width(p) + 2 * page_right_offset;
                break;
            case dir_LT_:
            case dir_LB_:
                cur_page_size.h = height(p) + depth(p) + 2 * page_left_offset;
                break;
            case dir_RT_:
            case dir_RB_:
                cur_page_size.h = height(p) + depth(p) + 2 * page_right_offset;
                break;
            }
        }
        if (page_height > 0)
            cur_page_size.v = page_height;
        else {
            switch (box_direction(page_direction)) {
            case dir_TL_:
            case dir_TR_:
                cur_page_size.v = height(p) + depth(p) + 2 * page_top_offset;
                break;
            case dir_BL_:
            case dir_BR_:
                cur_page_size.v = height(p) + depth(p) + 2 * page_bottom_offset;
                break;
            case dir_LT_:
            case dir_RT_:
                cur_page_size.v = width(p) + 2 * page_top_offset;
                break;
            case dir_LB_:
            case dir_RB_:
                cur_page_size.v = width(p) + 2 * page_bottom_offset;
                break;
            }
        }

        /* Think in upright page/paper coordinates (page origin = lower left edge).
           First preset |refpoint.pos| to the DVI origin (near upper left page edge). */

        switch (pdf->o_mode) {
        case OMODE_DVI:
            refpoint.pos.h = one_true_inch;
            refpoint.pos.v = cur_page_size.v - one_true_inch;
            dvi = refpoint.pos;
            break;
        case OMODE_PDF:
            refpoint.pos.h = pdf_h_origin;
            refpoint.pos.v = cur_page_size.v - pdf_v_origin;
            break;
        default:
            assert(0);
        }

        /* Then shift |refpoint.pos| of the DVI origin depending on the
           |page_direction| within the upright (TLT) page coordinate system */

        switch (box_direction(page_direction)) {
        case dir_TL_:
        case dir_LT_:
            refpoint.pos.h += h_offset;
            refpoint.pos.v -= v_offset;
            break;
        case dir_TR_:
        case dir_RT_:
            refpoint.pos.h +=
                cur_page_size.h - page_right_offset - one_true_inch;
            refpoint.pos.v -= v_offset;
            break;
        case dir_BL_:
        case dir_LB_:
            refpoint.pos.h += h_offset;
            refpoint.pos.v -=
                cur_page_size.v - page_bottom_offset - one_true_inch;
            break;
        case dir_RB_:
        case dir_BR_:
            refpoint.pos.h +=
                cur_page_size.h - page_right_offset - one_true_inch;
            refpoint.pos.v -=
                cur_page_size.v - page_bottom_offset - one_true_inch;
            break;
        }

        /* Then switch to page box coordinate system; do |height(p)| movement,
           to get the location of the box origin. */

        pdf->posstruct->dir = page_direction;
        cur.h = 0;
        cur.v = height(p);
        synch_pos_with_cur(pdf->posstruct, &refpoint, cur);
    } else {                    /* shipping a /Form */
        assert(pdf->o_mode == OMODE_PDF);
        pdf->posstruct->dir = box_dir(p);
        switch (box_direction(pdf->posstruct->dir)) {
        case dir_TL_:
        case dir_TR_:
        case dir_BL_:
        case dir_BR_:
            cur_page_size.h = width(p);
            cur_page_size.v = height(p) + depth(p);
            break;
        case dir_LT_:
        case dir_RT_:
        case dir_LB_:
        case dir_RB_:
            cur_page_size.h = height(p) + depth(p);
            cur_page_size.v = width(p);
            break;
        }
        switch (box_direction(pdf->posstruct->dir)) {
        case dir_TL_:
            pdf->posstruct->pos.h = 0;
            pdf->posstruct->pos.v = depth(p);
            break;
        case dir_TR_:
            pdf->posstruct->pos.h = width(p);
            pdf->posstruct->pos.v = depth(p);
            break;
        case dir_BL_:
            pdf->posstruct->pos.h = 0;
            pdf->posstruct->pos.v = height(p);
            break;
        case dir_BR_:
            pdf->posstruct->pos.h = width(p);
            pdf->posstruct->pos.v = height(p);
            break;
        case dir_LT_:
            pdf->posstruct->pos.h = height(p);
            pdf->posstruct->pos.v = width(p);
            break;
        case dir_RT_:
            pdf->posstruct->pos.h = depth(p);
            pdf->posstruct->pos.v = width(p);
            break;
        case dir_LB_:
            pdf->posstruct->pos.h = height(p);
            pdf->posstruct->pos.v = 0;
        case dir_RB_:
            pdf->posstruct->pos.h = depth(p);
            pdf->posstruct->pos.v = 0;
            break;
        }
    }

    /* Now we are at the point on the page where the origin of the page box should go. */

    shipbox_refpos = pdf->posstruct->pos;       /* for \gleaders */

    switch (pdf->o_mode) {
    case OMODE_DVI:
        assert(shipping_page);
        dvi_begin_page();
        break;
    case OMODE_PDF:
        pdf_begin_page(pdf, shipping_page);
        break;
    default:
        assert(0);
    }

    switch (type(p)) {
    case vlist_node:
        vlist_out(pdf, p);
        break;
    case hlist_node:
        hlist_out(pdf, p);
        break;
    default:
        assert(0);
    }

    if (shipping_page)
        incr(total_pages);
    cur_s = -1;

    /* Finish shipping */

    switch (pdf->o_mode) {
    case OMODE_DVI:
        dvi_end_page();
        break;
    case OMODE_PDF:
        pdf_end_page(pdf, shipping_page);
        break;
    default:
        assert(0);
    }

  DONE:
    if ((tracing_output <= 0) && (post_callback_id == 0) && shipping_page) {
        print_char(']');
        update_terminal();
    }
    dead_cycles = 0;
    /* Flush the box from memory, showing statistics if requested */
    if ((tracing_stats > 1) && (pre_callback_id == 0)) {
        tprint_nl("Memory usage before: ");
        print_int(var_used);
        print_char('&');
        print_int(dyn_used);
        print_char(';');
    }
    flush_node_list(p);
    if ((tracing_stats > 1) && (post_callback_id == 0)) {
        tprint(" after: ");
        print_int(var_used);
        print_char('&');
        print_int(dyn_used);
        print_ln();
    }
    if (shipping_page && (post_callback_id > 0))
        (void) run_callback(post_callback_id, "->");

    /* Finish sheet {\sl Sync\TeX} information record */
    if (synctexoption == 1)
        synctex_teehs();
}
