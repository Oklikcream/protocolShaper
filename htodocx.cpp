#include "htodocx.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

// Преобразование типа переменной в длину байт
int BytesFromCType(const QString &cType) {
    if (cType == "quint8" || cType == "qint8") return 1;
    if (cType == "quint16" || cType == "qint16") return 2;
    if (cType == "quint32" || cType == "qint32") return 4;
    if (cType == "quint64" || cType == "qint64") return 8;
    if (cType == "float") return 4;
    if (cType == "double") return 8;
    return 0;
}

// Разбор комментариев
bool ParseComment(const QString &comment, HField &f)
{
    QRegularExpression commentRegex(R"(^\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*;\s*(.+?)\s*;$)");

    QRegularExpressionMatch cMatch = commentRegex.match(comment);
    if (cMatch.hasMatch())
    {
        // Имя
        f.name = cMatch.captured(1).trimmed();

        // Размерность
        f.dimension = cMatch.captured(2).trimmed();

        // ЦМР
        QString lsbStr = cMatch.captured(3).trimmed();
        bool ok;
        double lsbValue = lsbStr.toDouble(&ok);
        f.lsb = ok ? lsbValue : 1.0;

        // Значения
        QString values = cMatch.captured(4).trimmed();
        if (values.contains("="))
        {
            QStringList pairs = values.split(", ");
            for (QString &pair : pairs)
            {
                if (pair.contains("="))
                {
                    QStringList kv = pair.split("=");
                    if (kv.size() == 2)
                    {
                        bool valOk;
                        quint64 val = kv[1].toULongLong(&valOk);
                        if (valOk)
                            f.enumValues[kv[0].trimmed()] = val;
                    }
                }
            }
        }

        // Направление байт
        f.endian = cMatch.captured(5).trimmed();

        // Примечание
        f.note = cMatch.captured(6).trimmed();

        return true;
    }
    return false;
}

// Разбор заголовнчого файла
QList<HPacket> ParseH(const QString &inFilePath)
{
    QFile hFile(inFilePath);
    if (!hFile.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(
            QString("Не удалось открыть файл %1").arg(inFilePath).toStdString()
        );
    }
    // Считываем весь файл
    QTextStream stream(&hFile);
    QString hFileContent = stream.readAll();
    hFile.close();

    QList<HPacket> packets;
    QRegularExpression packetRegex(R"(#pragma\s+pack\(1\)\s*typedef\s+struct\s*\{([\s\S]*?)\}\s*(\w+)\s*;\s*#pragma\s+pack\(\))");
    QRegularExpression unionRegex(R"(typedef\s+union\s*\{$)");
    QRegularExpression unionNameRegex(R"(\s*(\w+)\s+\w+;)");
    QRegularExpression unionTypeRegex(R"((quint\d+)\s+\w+;)");
    QRegularExpression bitfieldRegex(R"((quint\d+)\s+(\w+)\s*:\s*(\d+)\s*;.*//(.*))");
    QRegularExpression fieldRegex(R"((quint\d+|qint\d+|float|double)\s+(\w+)\s*;.*//(.*))");

    packetRegex.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = packetRegex.globalMatch(hFileContent);

    // Регулярными выражениями определяем пакеты, поля, объединения
    while(it.hasNext())
    {
        QRegularExpressionMatch pMatch = it.next();
        QString packetBody = pMatch.captured(1).trimmed();
        QString packetName = pMatch.captured(2);
        HPacket pkt;
        pkt.name = packetName;

        QStringList lines = packetBody.split('\n');
        HGroup unionGroup;
        bool insideUnion = false;
        QString unionName;

        for (const QString &rawLine : lines) {

            QString line = rawLine.trimmed();
            if (line.isEmpty()) continue;

            // Объединеие
            QRegularExpressionMatch match = unionRegex.match(line);
            if (match.hasMatch())
            {
                insideUnion = true;
                unionGroup = HGroup();
                unionGroup.isUnion = true;
                continue;
            }

            if (insideUnion)
            {
                // Тип объединения
                match = unionTypeRegex.match(line);
                if (match.hasMatch())
                {
                    unionGroup.unionType = match.captured(1);
                    unionGroup.lengthBytes = BytesFromCType(unionGroup.unionType);
                    continue;
                }

                // Имя объединения
                match = unionNameRegex.match(line);
                if (match.hasMatch())
                {
                    unionGroup.unionName = match.captured(1);
                    insideUnion = false;
                    if (!unionGroup.fields.isEmpty())
                    {
                        pkt.groups.append(unionGroup);
                    }
                    continue;
                }

                // Битовые поля
                match = bitfieldRegex.match(line);
                if (match.hasMatch())
                {
                    HField f;
                    f.type = match.captured(1);
                    f.lengthBits = match.captured(3).toInt();

                    QString comment = match.captured(4).trimmed();
                    if (!ParseComment(comment, f))
                    {
                        throw std::runtime_error(
                            QString("Ошибка обработки комментария").toStdString()
                        );
                    }
                    unionGroup.fields.append(f);
                }
                continue;
            }

            // Обычные поля
            match = fieldRegex.match(line);
            if (match.hasMatch())
            {
                HField f;
                f.type = match.captured(1).trimmed();
                f.lengthBits = BytesFromCType(f.type) * 8;

                QString comment = match.captured(3).trimmed();
                if (!ParseComment(comment, f))
                {
                    throw std::runtime_error(
                        QString("Ошибка обработки комментария").toStdString()
                    );
                }

                HGroup grp;
                grp.isUnion = false;
                grp.fields.append(f);
                pkt.groups.append(grp);
            }
        }

        packets.append(pkt);
    }

    return  packets;
}

// Заполение ячейки текстом
void SetCellText(QAxObject* table, int row, int col, const QString& text, int fontSize = 9) {
    if (!table || table->isNull()) return;
    QAxObject* cell = table->querySubObject("Cell(int, int)", row, col);
    if (!cell || cell->isNull()) return;
    QAxObject* range = cell->querySubObject("Range");
    if (!range || range->isNull()) return;
    range->setProperty("Text", text);
    QAxObject* font = range->querySubObject("Font");
    if (!font || font->isNull()) return;
    font->setProperty("Size", fontSize);
}

// Установки границ таблицы
void SetTableBorders(QAxObject* table) {
    if (!table || table->isNull()) return;
    QAxObject* borders = table->querySubObject("Borders");
    if (!borders || borders->isNull()) return;
    borders->setProperty("InsideLineStyle", 1);
    borders->setProperty("OutsideLineStyle", 1);
}

void CreateUnionTable(QAxObject* selection, const HGroup &grp, int paramNumber) {
    selection->dynamicCall("TypeText(const QString&)",
    QString("\nТаблица union: %1 (параметр №%2)\n\n").arg(grp.unionName).arg(paramNumber));

    // Создание таблицы
    QAxObject* tables = selection->querySubObject("Tables");
    QAxObject* range = selection->querySubObject("Range");
    QAxObject* table = tables->querySubObject(
                "Add(QAxObject*, int, int, QVariant&, QVariant&)",
                range->asVariant(),
                grp.fields.size() + 1,
                6,
                QVariant(0),
                QVariant(0)
    );
    SetTableBorders(table);

    // Заполнение и форматирование заголовков
    QStringList headers = {"№ бита", "Передаваемая информация", "Количество бит",
                           "ЦМР", "Значения", "Примечание"};
    for (int col = 0; col < headers.size(); ++col) {
        QAxObject* cell = table->querySubObject("Cell(int, int)", 1, col + 1);
        QAxObject* cellRange = cell->querySubObject("Range");
        cellRange->setProperty("Text", headers[col]);
        cellRange->setProperty("Alignment", 1);
        QAxObject *font = cellRange->querySubObject("Font");
        font->setProperty("Size", 10);
    }

    int row = 2;
    int bitOffset = 0;
    // Заполнение полей составного параметра
    for (const HField &field : grp.fields) {
        // Номер бита (диапазон)
        QString bitRange = QString("%1-%2").arg(bitOffset).arg(bitOffset + field.lengthBits - 1);
        SetCellText(table, row, 1, bitRange);

        // Передаваемая информация
        SetCellText(table, row, 2, field.name);

        // Количество бит
        SetCellText(table, row, 3, QString::number(field.lengthBits));

        // ЦМР
        SetCellText(table, row, 4, QString::number(field.lsb));

        // Значения
        QString valStr;
        if (!field.enumValues.isEmpty()) {
            for (auto it = field.enumValues.begin(); it != field.enumValues.end(); ++it) {
                if (!valStr.isEmpty()) valStr += "\n";
                valStr += it.key() + "=" + QString::number(it.value());
            }
        }
        SetCellText(table, row, 5, valStr);

        // Примечание
        SetCellText(table, row, 6, field.note);

        bitOffset += field.lengthBits;
        row++;
    }
}

bool ConvertHtoDOCX(QAxObject* word, const QString &inFilePath, const QString &outFilePath)
{
    QList<HPacket> packets;
    try {
        packets = ParseH(inFilePath);
    }  catch (const std::exception &e) {
        qCritical() << e.what();
        return false;
    }
    if (packets.isEmpty()) {
        qWarning() << "No packets found in" << inFilePath;
        return false;
    }

    // Создание документа
    QAxObject* documents = word->querySubObject("Documents");
    QAxObject* document = documents->querySubObject("Add()");
    QAxObject* selection = word->querySubObject("Selection");

    // Перебор всех пакетов
    for (const HPacket &pkt : packets) {

        int paramNumber = 1;
        QList<HUnionInfo> allUnions;

        selection->dynamicCall("TypeText(const QString&)",
                               QString("Пакет: %1\n\n").arg(pkt.name));

        int totalRows = pkt.groups.size();
        if (totalRows == 0) continue;

        // Создание таблицы
        QAxObject* range = selection->querySubObject("Range");
        QAxObject* tables = document->querySubObject("Tables");
        QAxObject* table = tables->querySubObject(
                    "Add(QAxObject*, int, int, QVariant&, QVariant&)",
                    range->asVariant(),
                    totalRows + 1,
                    8,
                    QVariant(),
                    QVariant()
                );
        SetTableBorders(table);

        // Заполнение и форматирование заголовков
        QStringList headers = {"№ параметра", "Передаваемая информация", "Количество байт",
                               "Тип данных", "ЦМР", "Значения", "Направление байт", "Примечание"};
        for (int col = 0; col < headers.size(); ++col) {
            QAxObject* cell = table->querySubObject("Cell(int, int)", 1, col + 1);
            QAxObject* cellRange = cell->querySubObject("Range");
            cellRange->setProperty("Text", headers[col]);
            cellRange->setProperty("Alignment", 1);
            QAxObject *font = cellRange->querySubObject("Font");
            font->setProperty("Size", 10);
        }

        int row = 2;
        for (const HGroup &grp : pkt.groups) {
            // Заполнение составного параметра
            if (grp.isUnion && grp.fields.size() > 1) {
                SetCellText(table, row, 1, QString::number(paramNumber));
                SetCellText(table, row, 2, grp.unionName);
                SetCellText(table, row, 3, QString::number(grp.lengthBytes));
                SetCellText(table, row, 4, grp.unionType);
                SetCellText(table, row, 5, "");
                SetCellText(table, row, 6, "");
                SetCellText(table, row, 7, grp.fields[0].endian);
                SetCellText(table, row, 8, "См. таблицу union: " + grp.unionName);

                HUnionInfo info;
                info.grp = grp;
                info.paramNumber = paramNumber;
                allUnions.append(info);
            }
            // Заполнение обычного параметра
            else {
                for (const HField &field : grp.fields) {
                    SetCellText(table, row, 1, QString::number(paramNumber));

                    QString info = field.name;
                    if (!field.dimension.isEmpty()) {
                        info += ", " + field.dimension;
                    }
                    SetCellText(table, row, 2, info);

                    int bytes = field.lengthBits / 8;
                    SetCellText(table, row, 3, QString::number(bytes));
                    SetCellText(table, row, 4, field.type);
                    SetCellText(table, row, 5, QString::number(field.lsb));

                    QString valStr;
                    if (!field.enumValues.isEmpty()) {
                        for (auto it = field.enumValues.begin(); it != field.enumValues.end(); ++it) {
                            valStr += it.key() + "=" + QString::number(it.value()) + "\n";
                        }
                    }
                    SetCellText(table, row, 6, valStr);
                    SetCellText(table, row, 7, field.endian);
                    SetCellText(table, row, 8, field.note);

                }
            }
            row++;
            paramNumber++;
        }

        // Курсор в конец
        QAxObject* rangeEnd = document->querySubObject("Range()");
        if (rangeEnd && !rangeEnd->isNull()) {
            rangeEnd->dynamicCall("Collapse(int)", 0); // wdCollapseEnd
            rangeEnd->dynamicCall("Select()");
        }


        selection->dynamicCall("TypeParagraph()");

        // Создаём дополнительные таблицы дял составных параметров
        if (!allUnions.isEmpty()) {
            for (const HUnionInfo &inf : allUnions) {
                CreateUnionTable(selection, inf.grp, inf.paramNumber);

                rangeEnd = document->querySubObject("Range()");
                if (rangeEnd && !rangeEnd->isNull()) {
                    rangeEnd->dynamicCall("Collapse(int)", 0); // wdCollapseEnd
                    rangeEnd->dynamicCall("Select()");
                }

                selection->dynamicCall("TypeParagraph()");
            }
        }
        selection->dynamicCall("TypeParagraph()");
    }

    // Сохраняем документ
    document->dynamicCall("SaveAs(const QString&, int)", outFilePath, 16);
    document->dynamicCall("Close()");

    return 1;
}
