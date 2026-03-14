<?php

date_default_timezone_set("Europe/Minsk");

if (isset($_POST['d'])) {

    $data = $_POST['d'];

    $line = $data . "|" . date("Y-m-d H:i:s") . "\n";

    file_put_contents("device_status.txt", $line);

    echo "OK";

} else {

    echo "NO_DATA";

}

?>