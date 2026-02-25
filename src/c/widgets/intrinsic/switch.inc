case COGITO_SWITCH:
      // Keep a usable track width even when label text is present.
      w = (n->text ? (text_w + 8) : 0) + 64;
      h = text_h + 8;
      if (h < 28) h = 28;
      break;
    
