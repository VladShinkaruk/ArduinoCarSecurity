<?php

$state = $_GET['state'] ?? '';

$state = strtoupper(trim($state));

if ($state == "ON" || $state == "OFF") {

    file_put_contents("status.txt", $state);

    echo "OK";

} else {

    echo "INVALID";

}

?>