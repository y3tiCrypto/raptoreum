// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PAPERWALLETDIALOG_H
#define BITCOIN_QT_PAPERWALLETDIALOG_H

#include <QDialog>
#include <QImage>
#include <QString>

class PaperWalletDialog : public QDialog {
    Q_OBJECT

public:
    explicit PaperWalletDialog(QWidget *parent, const QString &address, const QString &label, const QString &privateKeyWIF);
    ~PaperWalletDialog();

private Q_SLOTS:
    void saveImage();

private:
    QString address;
    QString label;
    QString privateKeyWIF;

    QImage generateCardImage();
    QImage generateQRCode(const QString &str, int size);
};

#endif // BITCOIN_QT_PAPERWALLETDIALOG_H
