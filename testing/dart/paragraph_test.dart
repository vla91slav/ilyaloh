// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:litetest/litetest.dart';

void main() {
  // Ahem font uses a constant ideographic/alphabetic baseline ratio.
  const double kAhemBaselineRatio = 1.25;

  test('predictably lays out a single-line paragraph', () {
    for (final double fontSize in <double>[10.0, 20.0, 30.0, 40.0]) {
      final ParagraphBuilder builder = ParagraphBuilder(ParagraphStyle(
        fontFamily: 'Ahem',
        fontStyle: FontStyle.normal,
        fontWeight: FontWeight.normal,
        fontSize: fontSize,
      ));
      builder.addText('Test');
      final Paragraph paragraph = builder.build();
      paragraph.layout(const ParagraphConstraints(width: 400.0));

      expect(paragraph.height, closeTo(fontSize, 0.001));
      expect(paragraph.width, closeTo(400.0, 0.001));
      expect(paragraph.minIntrinsicWidth, closeTo(fontSize * 4.0, 0.001));
      expect(paragraph.maxIntrinsicWidth, closeTo(fontSize * 4.0, 0.001));
      expect(paragraph.alphabeticBaseline, closeTo(fontSize * .8, 0.001));
      expect(
        paragraph.ideographicBaseline,
        closeTo(paragraph.alphabeticBaseline * kAhemBaselineRatio, 0.001),
      );
    }
  });

  test('predictably lays out a multi-line paragraph', () {
    for (final double fontSize in <double>[10.0, 20.0, 30.0, 40.0]) {
      final ParagraphBuilder builder = ParagraphBuilder(ParagraphStyle(
        fontFamily: 'Ahem',
        fontStyle: FontStyle.normal,
        fontWeight: FontWeight.normal,
        fontSize: fontSize,
      ));
      builder.addText('Test Ahem');
      final Paragraph paragraph = builder.build();
      paragraph.layout(ParagraphConstraints(width: fontSize * 5.0));

      expect(paragraph.height, closeTo(fontSize * 2.0, 0.001)); // because it wraps
      expect(paragraph.width, closeTo(fontSize * 5.0, 0.001));
      expect(paragraph.minIntrinsicWidth, closeTo(fontSize * 4.0, 0.001));

      // TODO(yjbanov): see https://github.com/flutter/flutter/issues/21965
      expect(paragraph.maxIntrinsicWidth, closeTo(fontSize * 9.0, 0.001));
      expect(paragraph.alphabeticBaseline, closeTo(fontSize * .8, 0.001));
      expect(
        paragraph.ideographicBaseline,
        closeTo(paragraph.alphabeticBaseline * kAhemBaselineRatio, 0.001),
      );
    }
  });

  test('getLineBoundary', () {
    const double fontSize = 10.0;
    final ParagraphBuilder builder = ParagraphBuilder(ParagraphStyle(
      fontFamily: 'Ahem',
      fontStyle: FontStyle.normal,
      fontWeight: FontWeight.normal,
      fontSize: fontSize,
    ));
    builder.addText('Test Ahem');
    final Paragraph paragraph = builder.build();
    paragraph.layout(ParagraphConstraints(width: fontSize * 5.0));

    // Wraps to two lines.
    expect(paragraph.height, closeTo(fontSize * 2.0, 0.001));

    final TextPosition wrapPositionDown = TextPosition(
      offset: 5,
      affinity: TextAffinity.downstream,
    );
    TextRange line = paragraph.getLineBoundary(wrapPositionDown);
    expect(line.start, 5);
    expect(line.end, 9);

    final TextPosition wrapPositionUp = TextPosition(
      offset: 5,
      affinity: TextAffinity.upstream,
    );
    line = paragraph.getLineBoundary(wrapPositionUp);
    expect(line.start, 0);
    expect(line.end, 5);

    final TextPosition wrapPositionStart = TextPosition(
      offset: 0,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(wrapPositionStart);
    expect(line.start, 0);
    expect(line.end, 5);

    final TextPosition wrapPositionEnd = TextPosition(
      offset: 9,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(wrapPositionEnd);
    expect(line.start, 5);
    expect(line.end, 9);
  });

  test('getLineBoundary RTL', () {
    const double fontSize = 10.0;
    final ParagraphBuilder builder = ParagraphBuilder(ParagraphStyle(
      fontFamily: 'Ahem',
      fontStyle: FontStyle.normal,
      fontWeight: FontWeight.normal,
      fontSize: fontSize,
      textDirection: TextDirection.rtl,
    ));
    builder.addText('القاهرةالقاهرة');
    final Paragraph paragraph = builder.build();
    paragraph.layout(ParagraphConstraints(width: fontSize * 5.0));

    // Wraps to three lines.
    expect(paragraph.height, closeTo(fontSize * 3.0, 0.001));

    final TextPosition wrapPositionDown = TextPosition(
      offset: 5,
      affinity: TextAffinity.downstream,
    );
    TextRange line = paragraph.getLineBoundary(wrapPositionDown);
    expect(line.start, 5);
    expect(line.end, 10);

    final TextPosition wrapPositionUp = TextPosition(
      offset: 5,
      affinity: TextAffinity.upstream,
    );
    line = paragraph.getLineBoundary(wrapPositionUp);
    expect(line.start, 0);
    expect(line.end, 5);

    final TextPosition wrapPositionStart = TextPosition(
      offset: 0,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(wrapPositionStart);
    expect(line.start, 0);
    expect(line.end, 5);

    final TextPosition wrapPositionEnd = TextPosition(
      offset: 9,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(wrapPositionEnd);
    expect(line.start, 5);
    expect(line.end, 10);
  });

  test('getLineBoundary empty line', () {
    const double fontSize = 10.0;
    final ParagraphBuilder builder = ParagraphBuilder(ParagraphStyle(
      fontFamily: 'Ahem',
      fontStyle: FontStyle.normal,
      fontWeight: FontWeight.normal,
      fontSize: fontSize,
      textDirection: TextDirection.rtl,
    ));
    builder.addText('Test\n\nAhem');
    final Paragraph paragraph = builder.build();
    paragraph.layout(ParagraphConstraints(width: fontSize * 5.0));

    // Three lines due to line breaks, with the middle line being empty.
    expect(paragraph.height, closeTo(fontSize * 3.0, 0.001));

    final TextPosition emptyLinePosition = TextPosition(
      offset: 5,
      affinity: TextAffinity.downstream,
    );
    TextRange line = paragraph.getLineBoundary(emptyLinePosition);
    expect(line.start, 5);
    expect(line.end, 5);

    // Since these are hard newlines, TextAffinity has no effect here.
    final TextPosition emptyLinePositionUpstream = TextPosition(
      offset: 5,
      affinity: TextAffinity.upstream,
    );
    line = paragraph.getLineBoundary(emptyLinePositionUpstream);
    expect(line.start, 5);
    expect(line.end, 5);

    final TextPosition endOfFirstLinePosition = TextPosition(
      offset: 4,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(endOfFirstLinePosition);
    expect(line.start, 0);
    expect(line.end, 4);

    final TextPosition startOfLastLinePosition = TextPosition(
      offset: 6,
      affinity: TextAffinity.downstream,
    );
    line = paragraph.getLineBoundary(startOfLastLinePosition);
    expect(line.start, 6);
    expect(line.end, 10);
  });
}
