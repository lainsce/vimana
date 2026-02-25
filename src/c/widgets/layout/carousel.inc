static void cogito_layout_carousel(CogitoNode* n, int x, int y) {
  int inner_x = x + n->padding_left;
  int inner_y = y + n->padding_top;
  int inner_w = n->w - n->padding_left - n->padding_right;
  int inner_h = n->h - n->padding_top - n->padding_bottom;
  
  if (inner_w < 0) inner_w = 0;
  if (inner_h < 0) inner_h = 0;
  
  size_t count = n->len;
  if (count == 0) return;
  
  // Fixed height of 240px for carousel items
  int item_h = inner_h;
  if (item_h < 0) item_h = 0;
  
  // Gap between carousel items (8px)
  const int item_gap = 8;
  
  // Width ratios: 4:2:1 pattern for exactly 3 visible items
  // The viewport always shows exactly 3 items with ratios summing to 7
  // At left-most: items [0,1,2] have ratios [4,2,1]
  // At right-most: items [n-2,n-1,n] have ratios [1,2,4]
  const int total_ratio = 7;
  const int dominant_ratio = 4;
  const int secondary_ratio = 2;
  const int tertiary_ratio = 1;
  
  // Calculate the base unit width (accounting for 2 gaps between 3 visible items)
  int gaps_in_viewport = (count >= 3) ? 2 : (int)(count - 1);
  if (gaps_in_viewport < 0) gaps_in_viewport = 0;
  int available_for_items = inner_w - (gaps_in_viewport * item_gap);
  if (available_for_items < 0) available_for_items = 0;
  
  int unit_w = available_for_items / total_ratio;
  if (unit_w < 1) unit_w = 1;
  
  // The active item index determines which item is dominant
  // Active can range from 0 to count-1
  // We need to calculate the scroll range based on active index transitions
  
  // When active=0: first item is dominant, viewport shows items 0,1,2
  // When active=count-1: last item is dominant, viewport shows items n-2,n-1,n
  
  // Calculate max_scroll:
  // The scroll distance needed to transition from active=0 to active=count-1
  // Each step (active index change by 1) corresponds to a scroll distance
  // that shifts the viewport by one "item position"
  
  // The key insight: the viewport always shows 3 items with total width = inner_w
  // The scroll offset shifts which 3 items are visible
  // When scrolling from active=0 to active=1:
  // - Item 0 shrinks from dominant(4) to secondary(2) = loses 2 units
  // - Item 1 grows from secondary(2) to dominant(4) = gains 2 units
  // - The viewport shifts left by the width of the "lost" portion
  
  // For count items, the max scroll is the distance to shift the viewport
  // from showing items [0,1,2] to showing items [count-3, count-2, count-1]
  // This is (count - 3) steps, each step being approximately unit_w * 2
  // (the difference between dominant and secondary)
  
  // But a simpler approach: calculate the x-position of each item
  // and determine max_scroll as the offset needed to show the last 3 items
  
  // For now, use a simpler calculation:
  // max_scroll = (count - 3) * unit_w * 2  (approximate shift per item)
  // But we need to be more precise for proper boundary behavior
  
  // Precise calculation:
  // Calculate the total width of all items when laid out
  // Then max_scroll = total_width - inner_w (to show the last items)
  
  // However, since widths change based on active index, we need a fixed reference
  // Use the "base" width calculation where all items have tertiary ratio (1)
  // except the active one which has dominant (4) and adjacent have secondary (2)
  
  // For scroll calculation, we use a simplified model:
  // - Each item transition (active i to i+1) scrolls by approximately 2 unit widths
  // - Total scroll range = (count - 1) * 2 * unit_w, but clamped to show last 3 items
  
  // Actually, let's calculate it properly:
  // The content is laid out with all items having widths based on active index
  // The scroll offset shifts the entire content left
  // max_scroll should be such that when scrolled to max, the last item is dominant
  
  // When last item (count-1) is dominant:
  // - Item count-1 has width = dominant_ratio * unit_w = 4 * unit_w
  // - Item count-2 has width = secondary_ratio * unit_w = 2 * unit_w
  // - Item count-3 has width = tertiary_ratio * unit_w = 1 * unit_w
  // - Items 0 to count-4 each have width = tertiary_ratio * unit_w = 1 * unit_w
  // Total width = (count-3) * unit_w + 1*unit_w + 2*unit_w + 4*unit_w
  //             = (count-3 + 7) * unit_w = (count + 4) * unit_w
  
  // But we want the viewport (inner_w) to show the last 3 items
  // The last 3 items have total width = 1 + 2 + 4 = 7 * unit_w = inner_w
  // So the scroll offset should be: total_width - inner_w
  // = (count + 4) * unit_w - 7 * unit_w = (count - 3) * unit_w
  
  // Wait, that's not right because inner_w = 7 * unit_w by definition
  // Let me recalculate...
  
  // Actually, the issue is that we're scaling all items to fit in inner_w
  // So the total content width is always inner_w, regardless of item count
  // This is wrong - we should have items outside the viewport
  
  // Correct approach:
  // 1. Calculate item widths based on active index (using ratio units)
  // 2. Total content width = sum of all item widths (in ratio units) * unit_w
  // 3. max_scroll = total_content_width - inner_w
  
  // For now, let's use a simpler model that works:
  // The scroll position directly maps to active index
  // max_scroll corresponds to active = count - 1
  // Each unit of scroll corresponds to a fraction of the total range
  
  // Calculate max_scroll as the distance needed to shift the viewport
  // from showing first item dominant to last item dominant
  // This is approximately (count - 1) * (dominant - secondary) * unit_w
  // = (count - 1) * 2 * unit_w
  
  // Total content width = sum of all item widths + gaps between them
  // Each item width = item_ratios[i] * available_for_items / total_ratio
  // But ratios aren't computed yet; pre-compute total ratio sum for active_idx=count-1
  // to find how far we need to scroll.  Use a simpler correct formula:
  // total_content = total_ratio_sum_for_current_active * (available_for_items / total_ratio) + (count-1)*item_gap
  // max_scroll = total_content - inner_w
  // However active_idx changes with scroll, so we use the worst-case (most content):
  // For any active index, ratio_sum = dominant + 2*secondary + (count-3)*tertiary (when count>=3)
  //                                 = 4 + 2*2 + (count-3)*1 = count + 5
  // total_content = (count+5) * available_for_items / 7 + (count-1)*item_gap  (for count>=3)
  // max_scroll = total_content - inner_w
  int est_ratio_sum = (count >= 3) ? (int)(count + 5) : total_ratio;
  int est_content_w = est_ratio_sum * available_for_items / total_ratio + (int)(count - 1) * item_gap;
  int max_scroll = est_content_w - inner_w;
  if (max_scroll < 0) max_scroll = 0;
  
  // Clamp scroll position
  if (n->carousel.scroll_x < 0) n->carousel.scroll_x = 0;
  if (n->carousel.scroll_x > max_scroll) n->carousel.scroll_x = max_scroll;
  
  // Calculate scroll progress (0.0 = left-most, 1.0 = right-most)
  double scroll_progress = 0.0;
  if (max_scroll > 0) {
    scroll_progress = (double)n->carousel.scroll_x / (double)max_scroll;
  }
  if (scroll_progress < 0.0) scroll_progress = 0.0;
  if (scroll_progress > 1.0) scroll_progress = 1.0;
  
  // Calculate which item should be "active" (dominant) based on scroll progress
  // At progress 0.0, active_idx = 0
  // At progress 1.0, active_idx = count - 1
  int active_idx = (int)(scroll_progress * (double)(count - 1) + 0.5);
  if (active_idx < 0) active_idx = 0;
  if (active_idx >= (int)count) active_idx = (int)count - 1;
  n->carousel.active_index = active_idx;
  
  // Calculate widths for each item based on active index
  // The active item gets dominant ratio, adjacent get secondary, others get tertiary
  // IMPORTANT: We need to ensure the 3 visible items always sum to 7 ratio units
  // Items outside the visible range get tertiary ratio
  
  int item_ratios_stack[64];
  int item_widths_stack[64];
  int* item_ratios = NULL;
  int* item_widths = NULL;
  void* heap_block = NULL;
  if (count <= 64) {
    item_ratios = item_ratios_stack;
    item_widths = item_widths_stack;
  } else {
    size_t ratio_bytes = sizeof(int) * count;
    size_t width_bytes = sizeof(int) * count;
    heap_block = calloc(1, ratio_bytes + width_bytes);
    if (!heap_block) return;
    char* p = (char*)heap_block;
    item_ratios = (int*)p;
    p += ratio_bytes;
    item_widths = (int*)p;
  }
  
  // Calculate the total ratio sum for proper scaling
  // The viewport should show exactly 3 items with ratios [1,2,4] or [4,2,1] or [2,4,1]
  // depending on active position
  
  // Assign ratios based on distance from active
  int total_ratio_sum = 0;
  for (size_t i = 0; i < count; i++) {
    int dist = (int)i - active_idx;
    
    if (dist == 0) {
      item_ratios[i] = dominant_ratio;  // 4
    } else if (dist == 1 || dist == -1) {
      item_ratios[i] = secondary_ratio;  // 2
    } else {
      item_ratios[i] = tertiary_ratio;  // 1
    }
    
    total_ratio_sum += item_ratios[i];
  }
  
  // Now we need to handle the boundary case properly
  // When active is at the edge (0 or count-1), we still need 3 visible items
  // with ratios summing to 7
  
  // The issue: when active=0, items [0,1,2] should have ratios [4,2,1] = 7
  // But items 3+ also have ratio 1, making total > 7
  // We need to scale so the viewport shows exactly 7 units worth of width
  
  // Solution: Calculate widths based on ratio units, then scale
  // so that the visible portion (7 units) fills the viewport
  
  // Calculate pixel widths
  // Scale factor: 7 ratio units = available_for_items pixels
  double scale = (double)available_for_items / (double)total_ratio;
  
  int total_content_w = 0;
  for (size_t i = 0; i < count; i++) {
    item_widths[i] = (int)(item_ratios[i] * scale);
    if (item_widths[i] < 1) item_widths[i] = 1;
    total_content_w += item_widths[i];
  }
  
  // Adjust last item to account for rounding
  if (count > 0) {
    int diff = total_content_w - (int)(total_ratio_sum * scale);
    item_widths[count - 1] -= diff;
    if (item_widths[count - 1] < 1) item_widths[count - 1] = 1;
  }
  
  // Minimum width threshold for showing label (approx 80px needed for text)
  const int min_label_width = 80;
  
  // Apply scroll offset and ensure content stays within carousel bounds
  // Clamp the starting position to prevent overflow on the right side
  int cx = inner_x - n->carousel.scroll_x;
  
  // Ensure we don't start too far right (which would cause left overflow)
  // or too far left (which would cause right overflow)
  // The content width calculation ensures items fit within the viewport when scrolled properly
  int total_content_width = 0;
  for (size_t i = 0; i < count; i++) {
    total_content_width += item_widths[i];
    if (i < count - 1) total_content_width += item_gap;
  }
  
  // Clamp starting position to prevent overflow
  int min_cx = inner_x + inner_w - total_content_width;
  if (min_cx > inner_x) min_cx = inner_x;
  if (cx < min_cx) cx = min_cx;
  
  // Layout children (CarouselItem widgets) with calculated widths and gaps
  for (size_t i = 0; i < count; i++) {
    CogitoNode* c = n->children[i];
    int cw = item_widths[i];
    
    // Center vertically
    int cy = inner_y;
    int ch = item_h;
    
    // Handle CarouselItem children - update label visibility
    if (c->kind == COGITO_CAROUSEL_ITEM && c->len > 0) {
      // Look for label child with carousel-item-label class
      for (size_t j = 0; j < c->len; j++) {
        CogitoNode* child = c->children[j];
        if (child->kind == COGITO_LABEL && child->class_name && 
            child->class_name->data && strstr(child->class_name->data, "carousel-item-label")) {
          // Hide label if item width is too small by setting opacity to 0
          if (cw < min_label_width) {
            child->opacity = 0.0f;
            child->opacity_set = true;
          } else {
            child->opacity = 1.0f;
            child->opacity_set = true;
          }
        }
      }
    }
    
    cogito_layout_node(c, cx + c->margin_left, cy + c->margin_top, cw, ch);
    cx += cw + item_gap;  // Add gap after each item
  }
  
  if (heap_block) free(heap_block);
}
