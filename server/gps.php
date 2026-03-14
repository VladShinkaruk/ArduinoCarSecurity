<?php

$lat = $_GET['lat'] ?? "";
$lon = $_GET['lon'] ?? "";

if($lat != "" && $lon != "")
{
    file_put_contents("status.txt", $lat . "," . $lon);
    echo "OK";
}
else
{
    echo "NO DATA";
}

?>