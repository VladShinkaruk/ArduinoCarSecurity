<?php
date_default_timezone_set("Europe/Minsk");

$event = isset($_GET['event']) ? $_GET['event'] : "unknown";
$coords = isset($_GET['coords']) ? $_GET['coords'] : "no_coords";
$sensor = isset($_GET['sensor']) ? $_GET['sensor'] : "unknown";

$line =
date("Y-m-d H:i:s")
." | "
.$event
." | "
.$sensor
." | "
.$coords
."\n";

file_put_contents("alarm_log.txt", $line, FILE_APPEND);

echo "OK";
?>