#pragma once

void drawCompose() {
  char displayBuffer[36];
  if (messageLen == 0) {
    strcpy(displayBuffer, "Type: _");
  } else {
    snprintf(displayBuffer, sizeof(displayBuffer), "%s_", typedMessage);
  }
  drawText(10, 52, displayBuffer, &FreeMonoBold9pt7b);

  char selectorBuffer[16];
  snprintf(selectorBuffer, sizeof(selectorBuffer), "<- [ %c ] ->", alphabet[currentLetterIdx]);
  drawText(75, 88, selectorBuffer, &FreeMonoBold9pt7b);

  drawText(12, 120, "[ <- ]", &FreeMonoBold9pt7b);
  drawText(105, 120, "[Select]", &FreeMonoBold9pt7b);
  drawText(212, 120, "[ -> ]", &FreeMonoBold9pt7b);
}

void handleComposeInput(bool leftPressed, bool selectPressed, bool rightPressed) {
  if (leftPressed) {
    currentLetterIdx--;
    if (currentLetterIdx < 0) currentLetterIdx = alphabetSize - 1;
    
    needRefresh = true;
    fastUpdate = true;
  } 
  else if (rightPressed) {
    currentLetterIdx++;
    if (currentLetterIdx >= alphabetSize) currentLetterIdx = 0;
    
    needRefresh = true;
    fastUpdate = true;
  } 
  else if (selectPressed) {
    char selectedChar = alphabet[currentLetterIdx];

    if (selectedChar == ' ' && messageLen > 0 && typedMessage[messageLen - 1] == ' ') {
      while (messageLen > 0 && typedMessage[messageLen - 1] == ' ') {
        typedMessage[--messageLen] = '\0';
      }
      currentState = STATE_SENT;
      needRefresh = true;
    } else {
      if (messageLen < 30) {
        typedMessage[messageLen] = selectedChar;
        messageLen++;
        typedMessage[messageLen] = '\0';
      }
      needRefresh = true;
      fastUpdate = true;
    }
  }
}