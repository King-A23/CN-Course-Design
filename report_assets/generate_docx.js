const fs = require("fs");
const path = require("path");
const {
  AlignmentType,
  BorderStyle,
  Document,
  Footer,
  HeadingLevel,
  ImageRun,
  LevelFormat,
  Packer,
  PageBreak,
  PageNumber,
  Paragraph,
  ShadingType,
  Table,
  TableCell,
  TableOfContents,
  TableRow,
  TextRun,
  WidthType,
} = require("docx");

const root = path.resolve(__dirname, "..");
const inputMd = path.join(root, "实验报告初稿.md");
const outputDocx = path.join(root, "计算机网络课程设计实验报告-DNS中继服务器.docx");

const A4 = { width: 11906, height: 16838 };
const margin = { top: 1440, right: 1440, bottom: 1440, left: 1440 };
const contentWidth = A4.width - margin.left - margin.right;

const cnFont = "宋体";
const headingCnFont = "黑体";
const enFont = "Times New Roman";
const codeFont = "Consolas";

function run(text, options = {}) {
  return new TextRun({
    text,
    size: options.size || 24,
    bold: options.bold || false,
    italics: options.italics || false,
    font: options.font || { ascii: enFont, hAnsi: enFont, eastAsia: cnFont },
  });
}

function paragraph(text = "", options = {}) {
  return new Paragraph({
    alignment: options.alignment,
    spacing: options.spacing || { line: 360, before: 60, after: 60 },
    indent: options.indent,
    children: [run(text, options.run || {})],
  });
}

function heading(text, level) {
  const map = {
    1: HeadingLevel.HEADING_1,
    2: HeadingLevel.HEADING_2,
    3: HeadingLevel.HEADING_3,
  };
  return new Paragraph({
    heading: map[level] || HeadingLevel.HEADING_3,
    spacing: { before: level === 1 ? 260 : 160, after: 120 },
    children: [
      new TextRun({
        text,
        bold: true,
        size: level === 1 ? 32 : level === 2 ? 28 : 26,
        font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont },
      }),
    ],
  });
}

function cell(text, width, header = false) {
  const border = { style: BorderStyle.SINGLE, size: 1, color: "9CA3AF" };
  return new TableCell({
    width: { size: width, type: WidthType.DXA },
    borders: { top: border, bottom: border, left: border, right: border },
    shading: header ? { fill: "D9D9D9", type: ShadingType.CLEAR } : undefined,
    margins: { top: 100, bottom: 100, left: 120, right: 120 },
    children: [
      new Paragraph({
        spacing: { before: 0, after: 0, line: 300 },
        children: [run(text, { bold: header, size: 21 })],
      }),
    ],
  });
}

function tableFromRows(rows) {
  const colCount = Math.max(...rows.map((r) => r.length));
  const base = Math.floor(contentWidth / colCount);
  const widths = Array.from({ length: colCount }, (_, i) => (i === colCount - 1 ? contentWidth - base * (colCount - 1) : base));
  return new Table({
    width: { size: contentWidth, type: WidthType.DXA },
    columnWidths: widths,
    rows: rows.map((row, rowIndex) => new TableRow({
      children: widths.map((w, i) => cell(row[i] || "", w, rowIndex === 0)),
    })),
  });
}

function parseTable(lines, start) {
  const rows = [];
  let i = start;
  while (i < lines.length && /^\s*\|.*\|\s*$/.test(lines[i])) {
    const line = lines[i].trim();
    if (!/^\|\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?$/.test(line)) {
      rows.push(line.replace(/^\|/, "").replace(/\|$/, "").split("|").map((s) => s.trim().replace(/`/g, "")));
    }
    i += 1;
  }
  return { table: tableFromRows(rows), next: i };
}

function codeBlock(code) {
  const lines = code.split(/\r?\n/);
  return lines.map((line) => new Paragraph({
    spacing: { before: 0, after: 0, line: 260 },
    shading: { fill: "F2F2F2", type: ShadingType.CLEAR },
    border: { left: { style: BorderStyle.SINGLE, size: 8, color: "BFBFBF", space: 4 } },
    children: [
      new TextRun({
        text: line.length ? line : " ",
        size: 18,
        font: { ascii: codeFont, hAnsi: codeFont, eastAsia: "等线" },
      }),
    ],
  }));
}

function imageSize(svgText) {
  const widthMatch = svgText.match(/width="([0-9.]+)(?:pt|px)?"/);
  const heightMatch = svgText.match(/height="([0-9.]+)(?:pt|px)?"/);
  let width = widthMatch ? Number(widthMatch[1]) : 0;
  let height = heightMatch ? Number(heightMatch[1]) : 0;
  if (!width || !height) {
    const viewBoxMatch = svgText.match(/viewBox="([^"]+)"/);
    if (viewBoxMatch) {
      const parts = viewBoxMatch[1].trim().split(/\s+/).map(Number);
      if (parts.length === 4 && Number.isFinite(parts[2]) && Number.isFinite(parts[3])) {
        width = parts[2];
        height = parts[3];
      }
    }
  }
  if (!width || !height) {
    width = 1180;
    height = 520;
  }
  const targetWidth = 455;
  return { width: targetWidth, height: Math.round(targetWidth * height / width) };
}

function imageParagraph(alt, rawPath) {
  let imagePath = path.resolve(root, rawPath);
  const pngCandidate = imagePath + ".png";
  if (path.extname(imagePath).toLowerCase() === ".svg" && fs.existsSync(pngCandidate)) {
    imagePath = pngCandidate;
  }
  const data = fs.readFileSync(imagePath);
  const ext = path.extname(imagePath).slice(1).toLowerCase();
  const sourceForSize = path.resolve(root, rawPath);
  const size = path.extname(sourceForSize).toLowerCase() === ".svg" && fs.existsSync(sourceForSize)
    ? imageSize(fs.readFileSync(sourceForSize, "utf8"))
    : { width: 455, height: 260 };
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 120, after: 60 },
      children: [
        new ImageRun({
          type: ext,
          data,
          transformation: size,
          altText: { title: alt, description: alt, name: alt },
        }),
      ],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 0, after: 120 },
      children: [run(alt, { size: 20 })],
    }),
  ];
}

function splitInline(text) {
  const children = [];
  const parts = text.split(/(`[^`]+`)/g);
  for (const part of parts) {
    if (!part) continue;
    if (part.startsWith("`") && part.endsWith("`")) {
      children.push(new TextRun({
        text: part.slice(1, -1),
        size: 22,
        font: { ascii: codeFont, hAnsi: codeFont, eastAsia: "等线" },
        shading: { fill: "F2F2F2", type: ShadingType.CLEAR },
      }));
    } else {
      children.push(run(part));
    }
  }
  return children;
}

function parseMarkdown(md) {
  const bodyMarker = "<!-- DOCX_BODY_START -->";
  const body = md.includes(bodyMarker) ? md.slice(md.indexOf(bodyMarker) + bodyMarker.length) : md;
  const lines = body.split(/\r?\n/);
  const children = [];
  let i = 0;
  while (i < lines.length) {
    const line = lines[i];
    if (!line.trim()) {
      i += 1;
      continue;
    }
    const img = line.match(/^!\[([^\]]*)\]\(([^)]+)\)$/);
    if (img) {
      children.push(...imageParagraph(img[1], img[2]));
      i += 1;
      continue;
    }
    const h = line.match(/^(#{1,3})\s+(.+)$/);
    if (h) {
      children.push(heading(h[2], h[1].length));
      i += 1;
      continue;
    }
    if (/^\s*\|.*\|\s*$/.test(line) && i + 1 < lines.length && /^\s*\|?\s*:?-{3,}:?/.test(lines[i + 1])) {
      const parsed = parseTable(lines, i);
      children.push(parsed.table);
      children.push(paragraph(""));
      i = parsed.next;
      continue;
    }
    if (line.startsWith("```")) {
      const code = [];
      i += 1;
      while (i < lines.length && !lines[i].startsWith("```")) {
        code.push(lines[i]);
        i += 1;
      }
      if (i < lines.length) i += 1;
      children.push(...codeBlock(code.join("\n")));
      children.push(paragraph(""));
      continue;
    }
    if (/^\d+\.\s+/.test(line)) {
      children.push(new Paragraph({
        spacing: { line: 340, before: 40, after: 40 },
        children: splitInline(line),
      }));
      i += 1;
      continue;
    }
    if (/^-\s+/.test(line)) {
      children.push(new Paragraph({
        numbering: { reference: "bullets", level: 0 },
        spacing: { line: 340, before: 30, after: 30 },
        children: splitInline(line.replace(/^-\s+/, "")),
      }));
      i += 1;
      continue;
    }
    const parts = [line.trim()];
    i += 1;
    while (i < lines.length && lines[i].trim() && !/^(#{1,3})\s+/.test(lines[i]) && !/^```/.test(lines[i]) && !/^\s*\|.*\|\s*$/.test(lines[i]) && !/^!\[/.test(lines[i])) {
      parts.push(lines[i].trim());
      i += 1;
    }
    children.push(new Paragraph({
      spacing: { line: 360, before: 60, after: 80 },
      firstLine: 480,
      children: splitInline(parts.join("")),
    }));
  }
  return children;
}

function coverChildren() {
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 900, after: 300 },
      children: [new TextRun({ text: "北京邮电大学课程设计报告", bold: true, size: 44, font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont } })],
    }),
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 120, after: 600 },
      children: [new TextRun({ text: "DNS 中继服务器", bold: true, size: 36, font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont } })],
    }),
    tableFromRows([
      ["课程设计名称", "计算机网络课程设计 - DNS 中继服务器"],
      ["学院", "待填写"],
      ["指导教师", "待填写"],
      ["班级", "待填写"],
    ]),
    paragraph(""),
    tableFromRows([
      ["班内序号", "学号", "学生姓名", "主要分工", "成绩"],
      ["待填写", "2024211129", "安宇辰", "协议解析、本地表、本地应答、TTL 提取/修补", "待填写"],
      ["待填写", "2024211130", "许久新", "事件循环、ID 转换、上游转发、Cache、测试验证", "待填写"],
    ]),
    paragraph(""),
    paragraph("课程设计内容：实现一个 C 语言 DNS 中继服务器，支持本地解析、不良网站拦截、上游中继、多客户端 ID 转换、超时处理、LRU Cache、TTL 递减和跨平台运行。"),
    new Paragraph({ children: [new PageBreak()] }),
    heading("目录", 1),
    new TableOfContents("目录", { hyperlink: true, headingStyleRange: "1-3" }),
    new Paragraph({ children: [new PageBreak()] }),
  ];
}

async function main() {
  const md = fs.readFileSync(inputMd, "utf8");
  const children = [...coverChildren(), ...parseMarkdown(md)];
  const doc = new Document({
    styles: {
      default: {
        document: {
          run: { size: 24, font: { ascii: enFont, hAnsi: enFont, eastAsia: cnFont } },
          paragraph: { spacing: { line: 360 } },
        },
      },
      paragraphStyles: [
        {
          id: "Heading1",
          name: "Heading 1",
          basedOn: "Normal",
          next: "Normal",
          quickFormat: true,
          run: { bold: true, size: 32, font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont } },
          paragraph: { spacing: { before: 260, after: 120 }, outlineLevel: 0 },
        },
        {
          id: "Heading2",
          name: "Heading 2",
          basedOn: "Normal",
          next: "Normal",
          quickFormat: true,
          run: { bold: true, size: 28, font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont } },
          paragraph: { spacing: { before: 180, after: 100 }, outlineLevel: 1 },
        },
        {
          id: "Heading3",
          name: "Heading 3",
          basedOn: "Normal",
          next: "Normal",
          quickFormat: true,
          run: { bold: true, size: 26, font: { ascii: enFont, hAnsi: enFont, eastAsia: headingCnFont } },
          paragraph: { spacing: { before: 140, after: 80 }, outlineLevel: 2 },
        },
      ],
    },
    numbering: {
      config: [
        {
          reference: "bullets",
          levels: [
            {
              level: 0,
              format: LevelFormat.BULLET,
              text: "•",
              alignment: AlignmentType.LEFT,
              style: { paragraph: { indent: { left: 520, hanging: 260 } } },
            },
          ],
        },
      ],
    },
    sections: [
      {
        properties: { page: { size: A4, margin } },
        footers: {
          default: new Footer({
            children: [
              new Paragraph({
                alignment: AlignmentType.CENTER,
                children: [run("第 ", { size: 20 }), new TextRun({ children: [PageNumber.CURRENT], size: 20 }), run(" 页", { size: 20 })],
              }),
            ],
          }),
        },
        children,
      },
    ],
  });
  const buffer = await Packer.toBuffer(doc);
  fs.writeFileSync(outputDocx, buffer);
  console.log(outputDocx);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
