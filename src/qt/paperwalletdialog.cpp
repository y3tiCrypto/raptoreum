// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/paperwalletdialog.h>

#include <qt/guiutil.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPen>
#include <QFileDialog>
#include <QMessageBox>

#if defined(HAVE_CONFIG_H)
#include <config/raptoreum-config.h> /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

PaperWalletDialog::PaperWalletDialog(QWidget *parent, const QString &address, const QString &label, const QString &privateKeyWIF) :
    QDialog(parent),
    address(address),
    label(label),
    privateKeyWIF(privateKeyWIF)
{
    setWindowTitle(tr("Print / Save Paper Wallet"));
    resize(640, 360);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(tr("This is a printable paper wallet card containing your public address and private key. Keep the private key safe and secret!"), this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Render preview image
    QImage cardImg = generateCardImage();
    QLabel *previewLabel = new QLabel(this);
    previewLabel->setPixmap(QPixmap::fromImage(cardImg.scaled(600, 225, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("border: 1px solid #d3d3d3; background-color: white;");
    layout->addWidget(previewLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton(tr("Save Image..."), this);
    QPushButton *closeBtn = new QPushButton(tr("Close"), this);

    connect(saveBtn, &QPushButton::clicked, this, &PaperWalletDialog::saveImage);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);
}

PaperWalletDialog::~PaperWalletDialog()
{
}

QImage PaperWalletDialog::generateCardImage()
{
    int w = 1000;
    int h = 375;
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(Qt::white);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw border
    QPen borderPen(QColor(47, 47, 47), 4);
    painter.setPen(borderPen);
    painter.drawRoundedRect(10, 10, w - 20, h - 20, 15, 15);

    // Draw header band
    QColor headerColor(40, 40, 40);
    painter.fillRect(QRect(12, 12, w - 24, 60), headerColor);

    // Title text
    painter.setPen(Qt::white);
    QFont titleFont("Arial", 18, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRect(30, 12, w - 60, 60), Qt::AlignVCenter | Qt::AlignLeft, "RAPTOREUM PAPER WALLET");

    // Draw vertical divider
    QPen dividerPen(QColor(180, 180, 180), 2, Qt::DashLine);
    painter.setPen(dividerPen);
    painter.drawLine(w / 2, 85, w / 2, h - 20);

    // Draw public side (left)
    painter.setPen(headerColor);
    QFont labelFont("Arial", 12, QFont::Bold);
    painter.setFont(labelFont);
    painter.drawText(QRect(30, 85, w/2 - 60, 30), Qt::AlignLeft, "PUBLIC ADDRESS (SHARE / RECEIVE)");

    // Generate and draw Address QR code
    QImage qrAddress = generateQRCode(address, 180);
    if (!qrAddress.isNull()) {
        painter.drawImage(30, 120, qrAddress);
    }

    // Draw address text
    QFont textFont("Courier", 10, QFont::Normal);
    painter.setFont(textFont);
    painter.drawText(QRect(220, 120, w/2 - 240, 180), Qt::TextWordWrap, address);

    // Draw private side (right)
    painter.setPen(QColor(200, 0, 0)); // Red for warning
    painter.setFont(labelFont);
    painter.drawText(QRect(w/2 + 30, 85, w/2 - 60, 30), Qt::AlignLeft, "PRIVATE KEY (KEEP SECRET!)");

    // Generate and draw Private Key QR code
    QImage qrPrivKey = generateQRCode(privateKeyWIF, 180);
    if (!qrPrivKey.isNull()) {
        painter.drawImage(w/2 + 30, 120, qrPrivKey);
    }

    // Draw private key text
    painter.setPen(headerColor);
    painter.setFont(textFont);
    painter.drawText(QRect(w/2 + 220, 120, w/2 - 240, 180), Qt::TextWordWrap, privateKeyWIF);

    // Draw note at the bottom
    painter.setPen(QColor(120, 120, 120));
    QFont noteFont("Arial", 8, QFont::Normal);
    painter.setFont(noteFont);
    painter.drawText(QRect(30, h - 35, w - 60, 20), Qt::AlignHCenter, "Always keep your private keys offline. Discard or shred copies you no longer use.");

    return img;
}

QImage PaperWalletDialog::generateQRCode(const QString &str, int size)
{
#ifdef USE_QRCODE
    QRcode *code = QRcode_encodeString(str.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code) return QImage();

    QImage myImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
    myImage.fill(Qt::white);
    unsigned char *p = code->data;
    for (int y = 0; y < code->width; y++) {
        for (int x = 0; x < code->width; x++) {
            myImage.setPixel(x + 4, y + 4, ((*p & 1) ? Qt::black : Qt::white));
            p++;
        }
    }
    QRcode_free(code);
    return myImage.scaled(size, size, Qt::KeepAspectRatio);
#else
    return QImage();
#endif
}

void PaperWalletDialog::saveImage()
{
    QString fn = QFileDialog::getSaveFileName(this, tr("Save Paper Wallet Image"), QString(), tr("PNG Image (*.png)"));
    if (!fn.isEmpty()) {
        if (generateCardImage().save(fn)) {
            QMessageBox::information(this, tr("Success"), tr("Paper wallet image saved successfully."));
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Failed to save paper wallet image."));
        }
    }
}
