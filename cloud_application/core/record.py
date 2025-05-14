from typing import Any
from core.database import Database

class Record:
    """
    The record class to store the sensor's data
    """

    solar = 0
    power = 0
    db = None

    @classmethod
    def set_db(cls, db: Any):
        """
        Set the database connection

        :param db: The database connection

        :return: None
        """
        cls.db = db

    @classmethod
    def set_power(cls, value):
        """
        Set the power value

        :param value: The power value

        :return: None
        """
        cls.power = value
        cls.check_all_values()

    @classmethod
    def set_solarpower(cls, value):
        """
        Set the panel's power value

        :param value: The panel's power value

        :return: None
        """
        cls.solar = value
        cls.check_all_values()
    @staticmethod
    def check_all_values():
        """
        Check if all values are set and insert the record into the database.

        :return: None
        """
        if all([Record.solar, Record.power, Record.voltage]):
            Record.insert_record()
            Record.solar = 0
            Record.power = 0
    @staticmethod
    def insert_record():
        """
        Insert the record into the database.

        :return: None
        """
        if Record.db is not None:
            db = Database()
        connection = db.connect_db()

        try:
            with connection.cursor() as cursor:
                sql = "INSERT INTO data (solar, power, voltage) VALUES (%s, %s, %s)"
                cursor.execute(sql, (Record.solar, Record.power, Record.voltage))
                connection.commit()
                print(f"Record inserted: {Record.__str__()}")
        except Exception as e:
            print(f"Error inserting record: {e}")

    @staticmethod
    def __str__():
        """
        Return the string representation of the record.

        :return: The string representation of the record
        """
        return f"solar: {Record.solar}, power: {Record.power}, voltage: {Record.voltage}"
