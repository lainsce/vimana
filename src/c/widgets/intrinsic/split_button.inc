case COGITO_SPLIT_BUTTON: {
  int tw = 0;
  if (n->text) tw = cogito_text_width(n->text->data);
  int btn_h = 0, pad = 0;
  cogito_button_size_metrics((int)n->split_btn.size, COGITO_BUTTON, &btn_h, &pad, NULL, NULL);
  int gap = 2;
  h = btn_h;
  int menu_w = h;
  int main_w = tw + pad * 2;
  if (main_w < 80) main_w = 80;
  w = main_w + menu_w + gap;
  break;
}
