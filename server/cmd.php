<?php

if(isset($_GET['set']))
{
    file_put_contents("status.txt", $_GET['set']);
    echo "SET";
}
else
{
    echo file_get_contents("status.txt");
}

?>