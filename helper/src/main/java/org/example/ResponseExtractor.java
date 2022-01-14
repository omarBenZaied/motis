package org.example;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;

public class ResponseExtractor {

  static final int RESPONSE_ID = 1;

  public static void main(String[] args) throws IOException {
    var lines = Files.readAllLines(Path.of("./verification/sbb-small/r-raptor_cpu-moc_bi.txt"));
    //var lines = Files.readAllLines(Path.of("./data/results/r-fwd-raptor_cpu-moc.txt"));

    var line = lines.get(RESPONSE_ID-1);
    Files.writeString(Path.of("./helper/responses.txt"), line);
  }

}
