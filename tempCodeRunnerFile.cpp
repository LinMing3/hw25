void pass_read_decision(int disk_id, int &action, int &times, int left_G)
{
    if (read_consume(disk_id) <= left_G)
    {
        action = READ;
        times = 1;
        return;
    }
    else
    {
        action = PASS;
        times = 1;
        return;
    }
}